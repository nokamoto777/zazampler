#pragma once
#include <juce_core/juce_core.h>
#include "FolderAccess.h"
#include "ZamplerBank.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>

// Only metadata is read here. Samples are loaded by the instrument loader on selection.
// Published snapshots are immutable; no disk I/O occurs on the audio thread.
class LibraryCatalog {
public:
    struct Entry { juce::File file; juce::String name; int presets=0; };
    struct Snapshot {
        juce::String root,bookmark;
        std::vector<Entry> entries;
        juce::Array<juce::File> instruments;
        juce::StringArray errors;
        uint64_t revision=0;
        bool scanning=false;
    };
    LibraryCatalog() : worker([this]{run();}) {}
    ~LibraryCatalog() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        changed.notify_one();worker.join();
    }
    void scan(const juce::String& root,const juce::String& bookmark) {
        std::lock_guard<std::mutex> lock(mutex);
        auto next=std::make_shared<Snapshot>();next->root=root;next->bookmark=bookmark;
        next->revision=++generation;next->scanning=root.isNotEmpty();current=next;
        pending=next->scanning;changed.notify_one();
    }
    std::shared_ptr<const Snapshot> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);return current;
    }
private:
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopping=false,pending=false;
    uint64_t generation=0;
    std::shared_ptr<const Snapshot> current=std::make_shared<Snapshot>();
    std::thread worker;
    bool cancelled(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex);return stopping || generation!=id;
    }
    void run() {
        for(;;) {
            Snapshot result;
            {std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[this]{return stopping || pending;});
             if(stopping)return;
             result=*current;pending=false;}
            try {
                FolderAccess access(result.root.toStdString(),result.bookmark.toStdString());
                result.root=juce::String::fromUTF8(access.path.c_str());result.bookmark=juce::String::fromUTF8(access.bookmark.c_str());
                const juce::File root(result.root);
                if(!root.isDirectory())result.errors.add("Library folder unavailable. Choose LIBRARY FOLDER again.");
                else for(const auto& entry:juce::RangedDirectoryIterator(root,true,"*",juce::File::findFiles,juce::File::FollowSymlinks::no)) {
                    if(cancelled(result.revision))break;
                    const auto file=entry.getFile();
                    if(file.hasFileExtension("sfz"))result.instruments.add(file);
                    if(!file.hasFileExtension("fxb;fxp"))continue;
                    try {
                        if(file.getSize()>4*1024*1024)throw std::runtime_error("Bank exceeds 4 MiB");
                        juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes))throw std::runtime_error("Cannot read bank");
                        const auto bank=ZamplerBank::parse(bytes.getData(),bytes.getSize());
                        int count=0;for(const auto& patch:bank.patches)if(patch.hasSample())++count;
                        result.entries.push_back({file,file.getRelativePathFrom(root),count});
                    } catch(const std::exception& e) {result.errors.add(file.getRelativePathFrom(root)+": "+e.what());}
                }
                result.instruments.sort();
                std::sort(result.entries.begin(),result.entries.end(),[](const Entry& a,const Entry& b){return a.name.compareNatural(b.name)<0;});
            } catch(const std::exception& e) {result.errors.add(e.what());}
            std::lock_guard<std::mutex> lock(mutex);
            if(!stopping && result.revision==generation) {
                result.scanning=false;current=std::make_shared<Snapshot>(std::move(result));
            }
        }
    }
};
