# NEIN

NEIN 在德文是「不」的意思：對 LINE 裡不想要的東西說「NEIN」。

NEIN 會修改 LINE 的 IPA，讓 iPhone 以副裝置身分登入，並可選擇移除廣告、隱藏推廣分頁等。

> NEIN to ads. NEIN to unnecessary clutter.<br>
> LINE, aber ohne Werbung. NEIN.


## 支援版本 📦

LINE 26.19.0，需要 iOS 18 以上。只接受已核對的 IPA，其他版本會拒絕處理。

macOS 版請看 [README-macOS.md](README-macOS.md)。

## 使用方式 📖

1. 準備解密過的 LINE IPA（沒有越獄裝置的話，請自行在網路上搜尋）。
2. 執行（推薦組合，開啟所有日常功能）：

   ```sh
   python3 tools/main.py --keychain-compat --remove-ads --collapse-friends-on-launch \
     jp.naver.line_26.19.0.ipa \
     output/NEIN-26.19.0.ipa
   ```

3. 用自己的憑證完整重簽後安裝，例如 [AltStore](https://altstore.io/) 或 [Sideloadly](https://sideloadly.io/)。重簽時請保留 Bundle ID `kinta.ma.nein`。

裝好的 App 預設叫 `NEIN`（可用 `--app-name`、`--app-icon` 自訂），可以和原版 LINE 並存。NEIN 收不到推播，建議保留原版 LINE 接收通知，平常用 NEIN。

預設以副裝置（偽裝成 iPad）登入。建議先用副裝置測試；若要用主帳號，請先備份所有資料。

## 選項 ⚙️

| 選項 | 說明 |
|---|---|
| `--keychain-compat` | 處理重簽後部分 Keychain 存取失敗的問題，建議開啟。 |
| `--remove-ads` | 停用廣告載入、隱藏廣告，並封鎖廣告網域（清單見 [`ad_domains.txt`](ad_domains.txt)）。 |
| `--collapse-friends-on-launch` | 首頁的好友區塊預設收合，仍可手動展開。 |
| `--skin-theme` | 把白色主題替換成指定的主題：格式 `TARGET_UUID=SOURCE_UUID`，套用中與系統退回預設的兩種狀態都渲染指定主題的外觀。 |
| `--primary-login` | 保留主手機的登入流程，不偽裝成 iPad。 |
| `--app-name 名稱` | 自訂主畫面上的 App 名稱，預設 `NEIN`。 |
| `--app-icon 圖檔` | 用自己的圖片當 App 圖示（PNG、JPEG、HEIC 等皆可）：置中裁成正方形，透明處補白，短邊至少 180 像素，建議 1024 像素。LINE 原本的圖示仍可在 App 內的圖示切換器換回。不能和 `--entry-only` 一起用。 |

例如自訂名稱與圖示：

```sh
python3 tools/main.py --keychain-compat --remove-ads --collapse-friends-on-launch \
  --app-name 我的LINE --app-icon my_icon.png \
  jp.naver.line_26.19.0.ipa \
  output/MyLINE-26.19.0.ipa
```

自訂名稱與圖示不會改變 Bundle ID，所以不同名稱的版本仍是同一個 App，無法同時安裝。

貼圖商店與主題小舖預設開啟，不用加參數：貼圖可以用在原版 LINE 儲值的代幣購買（曾於 26.17.0 實測），主題小舖已實測可開啟瀏覽、免費主題能下載並套用（付費主題購買未測），兩者在 NEIN 內都無法儲值。若某個 IPA 推不出已稽核的小舖 patch，建置會自動略過並記在 manifest 的 `shops_skipped`。

廣告網域清單可以用 `python3 tools/scan_ad_domains.py <IPA> ad_domains.txt` 重新產生。

## 原理 🔧

- 只修改登入入口的裝置判斷，讓 LINE 以為是 iPad，不會全域偽裝裝置。
- 注入相容層，處理重簽後 App Group 與 Keychain 無法使用的問題。

## 注意 ⚠️

- 不支援推播通知。
- 不包含原版的分享、Widget、Siri 與 Apple Watch 擴充功能。
- NEIN 不接收 LINE 連結，連結會由原版 LINE 開啟。
- `--remove-ads` 可能讓 LINE News、推薦內容或廣告影片無法載入。
- iPhone Duo 支援（實驗性）現在起變成預設內建，如在其他手機有版面問題請回報。
- LINE 26.19.0 原版已支援 iPhone Duo 的螢幕尺寸與相機，但還不支援 iPhone Duo 的側邊按鈕（系統直向選單）與展開後的雙欄版面，NEIN 仍會補上。
- 請先備份 LINE 資料，再於測試裝置安裝。

## 致謝 🙏

感謝 Threads 網友 [@xinquan.tw](https://www.threads.com/@xinquan.tw) 提供 iPhone Duo 直向選單的架構。

## 免責聲明 📄

本專案僅供學術研究、相容性測試與教育用途，為非官方研究與修改工具，與 LINE、NAVER 或 Apple 無關。使用修改後的 IPA 可能造成帳號、聊天資料或通知功能異常，也可能違反相關服務條款。請先備份資料，並確認使用方式符合適用法令及相關服務條款；使用者須自行承擔所有風險與責任。
