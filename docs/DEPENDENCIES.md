# 依存関係

| 名前 | 固定バージョン / コミット | 用途 |
|---|---|---|
| JUCE | 8.0.6 / `51a8a6d7aeae7326956d747737ccf1575e61e209` | GUI、MIDI、AUv3/AUv2/VST3ラッパー |
| sfizz | 1.2.3 / `4e70dc0bef53b41f2853ed46e26f5911114c92d0` | SFZ解析、サンプル再生 |

CMakeが初回にGitから取得します。sfizzのサブモジュールも各固定コミットを取得します。
ZIPに依存ソースやApple SDK、サードパーティ音色は含めません。
依存コードを更新する場合はAPI差分と回帰テストを確認してください。固定版であり最新版を意味しません。

独自コードはMITですが、JUCEはAGPLv3または商用ライセンスです。
**結合したプラグイン全体をMITとして配布できるわけではありません。**
配布時はJUCE、同梱VST3 SDK、sfizzおよびその依存関係のライセンス・ソース公開条件・表示義務を確認し、該当するライセンス全文を添付してください。
sfizz本体はBSD-2-Clauseですが、内部依存は複数のライセンスを含みます。

一次資料（ビルド取得後にも確認可能）：

- `build-macos/_deps/juce-src/LICENSE.md`
- `build-macos/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/`
- `build-macos/_deps/sfizz-src/LICENSE`
- `build-macos/_deps/sfizz-src/README.md` のDependencies and licenses
- sfizzの `external/` および `src/external/` 以下の各LICENSE

ビルドにAppleのフルXcodeが必要です。Apple SDKは本ZIPには含まれません。

## 0.8のsfizz拡張

`scripts/patch-sfizz.cmake`が固定版sfizz 1.2.3の3ソースへ検査付き・冪等の変更を加えます。SynthConfigに借用の音程バッファ、SynthMessagingに専用メッセージ、Voiceにサンプル単位の加算を追加。sfizz元ソースのBSD-2-Clauseライセンスは維持されます。
Engine::renderの直前にポインターを設定し、同期renderの直後に解除します。未設定時は従来処理と同じです。Mono時はボイスの元の鍵番号との差を加算し、リリース音も同じ演奏音程へ追従します。MIDI pitch bendのレンジは書き換えません。
外部のZAZAMPLER_SFIZZ_PATHを指定する場合は、その書き込み可能な専用チェックアウトへパッチを適用します。未知のソース配置はCMakeエラーとし、黙ってスキップしません。
