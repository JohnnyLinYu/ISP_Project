# ISP Pipeline
概述: 目的是幫助了解ISP Pipeline的背後原理，因此購買一些硬體來玩看看，順便練習一下怎麼控制一台相機。前面主要將重點放在如何操作這台相機以及如何可以實現live取像，後面的重點會在「如何利用相機端拍攝到的raw影像，一步步還原成肉眼見到的影像」，正常來說拿一張bayer原圖就可以寫一些ISP的演算法產出圖片，但我也想看看相機端一些硬體的問題，是怎麼影響到ISP要先去處理這樣的問題才能還原，才因而決定買個簡易的硬體來玩看看。
---

## Camera hardware
- 硬體: Rasberry Pi 5
- 相機: HQ camera
    - 感光元件:  Sony 的 IMX477R (CMOS)

不同於部分具備內建 Frame Buffer 的工業相機，HQ Camera 不提供讓程式管理完整影像暫存的通用記憶體。影像經 CSI-2 傳送至 Raspberry Pi，再由影像管線透過 DMA 等機制寫入系統 RAM 中的 Buffer。這種架構可以支援低複製成本的影像存取。

## 套件使用
這個專案使用 libcamera 作為相機控制與影像擷取框架，負責管理相機裝置、設定影像輸出格式、配置 FrameBuffer、提交 Request，以及接收擷取完成事件。同時，libcamera 能透過平台相關的 Pipeline Handler 協調底層相機硬體與 ISP，提供 RAW 或經過處理的影像資料。

## 流程
https://github.com/JohnnyLinYu/ISP_Project/issues/1#issue-6367274384

功能:
1. CameraManager: 是整個相機系統的一個入口與管理者。開始的時候他會搜尋硬體上可用的相機並調用，也能做到提供相機的清單。(補充: 一個程式不能創建多個CameraManager物件)
2. Camera: 是CameraManager中相機本身的物件，同時也能去控制相機的一些參數。
    - Stream: 代表從相機輸出的影像資料流。能夠在同一時間將感光元件拍到的畫面，處理成多種不同的版本。
    - 取得使用權: 使用前要先進行鎖定，鎖定成功才代表你有這個相機的使用權。
    - 設定參數: 雖說可以讓你自由設定參數，但一切還得由相機端說得準，因此設定完後，還是得讀出設定值看是否一致。
3. FrameBufferAllocator: 提供「內建記憶體緩衝區分配器。簡單來說就是提供一段記憶體供相機端使用。
4. Request: 代表一次影像擷取工作，需要綁定至少一組 Stream 與 FrameBuffer。透過 queueRequest() 提交擷取工作，完成後可以透過 reuse() 重用 Request，再次提交至 Camera，達到連續取像的目的。
    - 綁定畫面儲存空間: 每個 Request 必須至少連結一個Stream與對應的FrameBuffer
    - 傳遞逐幀控制參數
    - 回傳拍攝結果與詮釋資料
    - 資源追蹤與重複使用
5. callback: libcamera 使用 Signal-Slot 機制提供非同步事件通知。當 Request 完成時，會觸發 requestCompleted，並執行應用程式註冊的 Callback。由於 Callback 在 CameraManager Thread 中執行，應避免耗時或阻塞操作，以免影響相機管線的事件處理與即時效能。