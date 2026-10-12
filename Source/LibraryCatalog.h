#pragma once
#include <juce_core/juce_core.h>
#include "FolderAccess.h"
#include "ZamplerBank.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <map>
#include <set>

// Local metadata cache only: no sample data, no library traversal on cache restore.
class LibraryCatalog {
public:
    struct Entry { juce::File file; juce::String name; int presets=0; juce::Array<juce::File> patches; };
    struct Snapshot {
        juce::String root,bookmark;
        std::vector<Entry> entries;
        juce::Array<juce::File> instruments;
        juce::StringArray errors;
        uint64_t revision=0;
        bool scanning=false,cached=false;
    };
    static juce::File defaultCache() {
        const auto override=juce::SystemStats::getEnvironmentVariable("ZAZAMPLER_CACHE_DIR",{});
        if(override.isNotEmpty())return juce::File(override);
        auto base=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base=base.getChildFile("Application Support");
       #endif
        return base.getChildFile("ZaZampler/LibraryCache");
    }
    explicit LibraryCatalog(juce::File cache=defaultCache()) : cacheDirectory(cache) {
        if(auto xml=juce::XmlDocument::parse(cacheDirectory.getChildFile("last.xml")))
            restore(xml->getStringAttribute("root"),xml->getStringAttribute("bookmark"));
        worker=std::thread([this]{run();});
    }
    ~LibraryCatalog() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        changed.notify_one();worker.join();
    }
    void restore(const juce::String& root,const juce::String& bookmark) {
        if(root.isEmpty())return;
        if(auto cached=readCache(root,bookmark)) {
            std::lock_guard<std::mutex> lock(mutex);cached->revision=++generation;pending=false;current=cached;return;
        }
        scan(root,bookmark,false);
    }
    void scan(const juce::String& root,const juce::String& bookmark,bool remember=true) {
        std::lock_guard<std::mutex> lock(mutex);
        auto next=std::make_shared<Snapshot>();next->root=root;next->bookmark=bookmark;
        next->revision=++generation;next->scanning=root.isNotEmpty();current=next;
        if(remember && root.isNotEmpty()) {
            juce::XmlElement xml("Location");xml.setAttribute("root",root);xml.setAttribute("bookmark",bookmark);
            if(!writeXml(cacheDirectory.getChildFile("last.xml"),xml))next->errors.add("Cannot save library location to local cache.");
        }
        pending=next->scanning;changed.notify_one();
    }
    std::shared_ptr<const Snapshot> snapshot() const {std::lock_guard<std::mutex> lock(mutex);return current;}
private:
    juce::File cacheDirectory;
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopping=false,pending=false;
    uint64_t generation=0;
    std::shared_ptr<const Snapshot> current=std::make_shared<Snapshot>();
    std::thread worker;
    juce::File cacheFile(const juce::String& root) const {return cacheDirectory.getChildFile(juce::String::toHexString(root.hashCode64())+".xml");}
    static bool writeXml(const juce::File& file,const juce::XmlElement& xml) {
        file.getParentDirectory().createDirectory();juce::TemporaryFile temp(file);
        return xml.writeTo(temp.getFile()) && temp.overwriteTargetFileWithTemporary();
    }
    std::shared_ptr<Snapshot> readCache(const juce::String& root,const juce::String& bookmark) const {
        const auto file=cacheFile(root);if(file.getSize()>32*1024*1024)return {};
        auto xml=juce::XmlDocument::parse(file);
        if(!xml || !xml->hasTagName("Catalog") || xml->getIntAttribute("version")!=2 || xml->getStringAttribute("root")!=root)return {};
        auto s=std::make_shared<Snapshot>();s->root=root;s->bookmark=bookmark;s->cached=true;
        for(auto* n=xml->getFirstChildElement();n;n=n->getNextElement()) {
            if(!n->hasTagName("Error") && !juce::File::isAbsolutePath(n->getStringAttribute("path")))return {};
            if(n->hasTagName("Bank")) {
                Entry e{juce::File(n->getStringAttribute("path")),n->getStringAttribute("name"),n->getIntAttribute("presets"),{}};
                for(auto* p=n->getFirstChildElement();p;p=p->getNextElement()) {
                    if(!juce::File::isAbsolutePath(p->getStringAttribute("path")))return {};
                    e.patches.add(juce::File(p->getStringAttribute("path")));
                }
                s->entries.push_back(std::move(e));
            } else if(n->hasTagName("Instrument"))s->instruments.add(juce::File(n->getStringAttribute("path")));
            else if(n->hasTagName("Error"))s->errors.add(n->getStringAttribute("text"));
        }
        return s;
    }
    bool saveCache(const Snapshot& s) const {
        juce::XmlElement xml("Catalog");xml.setAttribute("version",2);xml.setAttribute("root",s.root);
        for(const auto& e:s.entries) {
            auto* n=xml.createNewChildElement("Bank");n->setAttribute("path",e.file.getFullPathName());n->setAttribute("name",e.name);n->setAttribute("presets",e.presets);
            for(const auto& p:e.patches)n->createNewChildElement("Patch")->setAttribute("path",p.getFullPathName());
        }
        for(const auto& f:s.instruments)xml.createNewChildElement("Instrument")->setAttribute("path",f.getFullPathName());
        for(const auto& e:s.errors)xml.createNewChildElement("Error")->setAttribute("text",e);
        return writeXml(cacheFile(s.root),xml);
    }
    bool cancelled(uint64_t id) const {std::lock_guard<std::mutex> lock(mutex);return stopping || generation!=id;}
    void run() {
        for(;;) {
            Snapshot result;
            {std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[this]{return stopping || pending;});if(stopping)return;result=*current;pending=false;}
            try {
                FolderAccess access(result.root.toStdString(),result.bookmark.toStdString());
                result.root=juce::String::fromUTF8(access.path.c_str());result.bookmark=juce::String::fromUTF8(access.bookmark.c_str());
                const juce::File root(result.root);
                std::vector<Entry> singles;
                std::map<juce::String,std::set<std::string>> signatures;
                if(!root.isDirectory())result.errors.add("Library folder unavailable. Reconnect the drive or choose LIBRARY FOLDER again.");
                else for(const auto& item:juce::RangedDirectoryIterator(root,true,"*",juce::File::findFiles,juce::File::FollowSymlinks::no)) {
                    if(cancelled(result.revision))break;
                    const auto file=item.getFile();
                    if(file.hasFileExtension("sfz;rex;rx2;rcy"))result.instruments.add(file);
                    if(!file.hasFileExtension("fxb;fxp"))continue;
                    try {
                        if(file.getSize()>4*1024*1024)throw std::runtime_error("Bank exceeds 4 MiB");
                        juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes))throw std::runtime_error("Cannot read bank");
                        const auto bank=ZamplerBank::parse(bytes.getData(),bytes.getSize());
                        int count=0;for(const auto& patch:bank.patches)if(patch.hasSample()) {
                            ++count;auto key=patch.name+std::string(1,'\0')+patch.sampleFile+std::string(1,'\0');
                            key.append(reinterpret_cast<const char*>(patch.parameters.data()),sizeof(patch.parameters));
                            signatures[file.getFullPathName()].insert(std::move(key));
                        }
                        Entry e{file,file.getRelativePathFrom(root),count,{}};
                        if(file.hasFileExtension("fxb"))result.entries.push_back(e);else singles.push_back(e);
                    } catch(const std::exception& e) {result.errors.add(file.getRelativePathFrom(root)+": "+e.what());}
                }
                // Associate FXPs with the nearest unambiguous ancestor bank. With no
                // such bank, group siblings by directory instead of listing every FXP.
                for(const auto& p:singles) {
                    int best=-1,length=-1;bool ambiguous=false;
                    for(size_t i=0;i<result.entries.size();++i) {
                        const auto& e=result.entries[i];if(!e.file.hasFileExtension("fxb"))continue;
                        const auto dir=e.file.getParentDirectory();if(!p.file.isAChildOf(dir))continue;
                        const int n=dir.getFullPathName().length();
                        if(n>length){best=static_cast<int>(i);length=n;ambiguous=false;}else if(n==length)ambiguous=true;
                    }
                    if(best<0 || ambiguous) {
                        const auto dir=p.file.getParentDirectory();best=-1;
                        for(size_t i=0;i<result.entries.size();++i)if(result.entries[i].file==dir)best=static_cast<int>(i);
                        if(best<0){best=static_cast<int>(result.entries.size());result.entries.push_back({dir,dir==root?root.getFileName():dir.getRelativePathFrom(root),0,{}});}
                    }
                    auto& e=result.entries[static_cast<size_t>(best)];e.patches.add(p.file);e.presets+=p.presets;
                }
                result.instruments.sort();for(auto& e:result.entries) {
                    e.patches.sort();auto unique=signatures[e.file.getFullPathName()];
                    for(const auto& p:e.patches){const auto& keys=signatures[p.getFullPathName()];unique.insert(keys.begin(),keys.end());}
                    e.presets=static_cast<int>(unique.size());
                }
                std::sort(result.entries.begin(),result.entries.end(),[](const Entry& a,const Entry& b){return a.name.compareNatural(b.name)<0;});
            } catch(const std::exception& e) {result.errors.add(e.what());}
            // Save before publishing so a second instance can immediately restore it.
            if(!cancelled(result.revision) && !saveCache(result))result.errors.add("Cannot save library index to local cache.");
            std::lock_guard<std::mutex> lock(mutex);
            if(!stopping && result.revision==generation){result.scanning=false;current=std::make_shared<Snapshot>(std::move(result));}
        }
    }
};
