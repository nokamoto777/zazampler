# GitHub ActionsでmacOSバイナリを作る

`macOS arm64 build` はmacos-15（Apple Silicon）で実行します。

## ReleaseのAssetsからダウンロード

1. [Releases](https://github.com/nokamoto777/zazampler/releases)を開き、利用するビルドの **Assets** を展開。
2. `ZaZampler-macOS-arm64.zip` を取得して展開。`Source code` はバイナリではありません。
3. 展開先へターミナルで移動し、DAWを終了して `bash scripts/install-macos.sh` を実行。
4. 起動したStandaloneを終了し、DAWを再起動してプラグインを再スキャン。

VST3 / AUv2 / AUv3入りStandalone、インストール・検証スクリプト、Demo、README、CHANGES、詳細ドキュメントを含みます。AUv3はアプリ内に保持してください。
Assetsの `SHA256SUMS.txt` を同じ場所に保存すると、`shasum -a 256 -c SHA256SUMS.txt` でZIPを検証できます。
ReleaseのAssetsはActionsの14日間の保存期限とは別に保持されます。

## 実行と公開の条件

| 起点 | ビルド・テスト | Release公開 |
|---|---|---|
| mainへのpush（PRマージを含む） | 実行 | `ci-実行番号` の開発版Pre-release |
| mainを選んで手動実行 | 実行 | `ci-実行番号` の開発版Pre-release |
| `v`で始まるタグのpush | 実行 | そのタグのRelease |
| main向けPR | 実行 | 公開せずArtifactsへ保存 |
| その他のブランチから手動実行 | 実行 | 公開せずArtifactsへ保存 |

ビルドジョブは読み取り権限のみ。成功した成果物を別ジョブが受け取り、そのジョブだけが `contents: write` でReleaseへ公開します。追加Secretsは不要です。PRを自動マージする処理はありません。
同じ実行を再実行した場合、同名Assetsを再アップロードします。開発版は実行番号ごとに保存し、バージョンReleaseのLatest表示を変更しません。
GitHubのリポジトリ／組織ポリシーがActionsのRelease作成を禁止している場合は公開ジョブが失敗します。

## PRのArtifacts

Actionsから成功した実行を開き、`ZaZampler-macOS-arm64-実行番号` をダウンロード（GitHubログインが必要、14日間保存）。外側のZIPと中の `ZaZampler-macOS-arm64.zip` を両方展開してください。
失敗時にもビルド・テスト・CMakeログを別Artifactへ保存します。

## 検証と署名

arm64 Releaseビルド、3件のCTest（エディターのサイズ変更とマウス座標を含む）、AUv3埋め込みとアプリ署名を確認します。パッケージ時にVST3/AUv2/Standaloneの署名・arm64形式も検査し、dittoで権限とバンドル構造を保ってZIP化します。

Apple Developer証明書不要のアドホック署名です。Developer ID署名・Apple公証済みではなく、macOSがダウンロードしたアプリやプラグインをブロックする場合があります。
自動テストとは別に、実際のDAWでの認識・AUv3登録・演奏・ホスト側リサイズを確認してください（[MAC_ACCEPTANCE.md](MAC_ACCEPTANCE.md)）。
