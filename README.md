# USBKBD2X68K

USBKBD2X68K とは USBキーボードをX68000で使えるようにしたものです。

## 注意
全てのUSB機器に対して稼働確認が取れているわけではありません。
相性等動かないものもありますのでご了承ください。
また、ゲーミングキーボード等電力を多く使用するものも使用できません

## 完成品
![](images/usbkbs2x68k1.jpg)
![](images/usbkbs2x68k2.jpg)

## ガーバーデータ
![](images/usbkbs2x68k3.jpg)
![](images/usbkbs2x68k4.jpg)

## 配線図
![](images/USBKBD2X68K.png)

## キーアサイン
109キーボードではすべてのキーをアサインすることができません。
ですので使用頻度の低いキーをアサインしていません。
日本語109キーボードアサイン例 `赤字部分がX68000のキーアサイン`
![](images/usbkbs2x68k.png)

## Setup
* Arduino Software (IDE) V1.8.12 を使用しています。
 ライブラリとしてUSB_Host_Shield_2.0 を使用しています。
  (https://github.com/felis/USB_Host_Shield_2.0) 
 ライブラリとしてMsTimer2 を使用しています。
  (http://playground.arduino.cc/Main/MsTimer2)
- Arduino Pro Mini 3.3V 8MHz を使用します


