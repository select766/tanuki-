# About this project

"The Minstrel's Ballad: Tanuki's Reign" (tanuki-wcsc27 ver) is a shogi engine (AI player) based on YaneuraOu.  The development concept is "easily and for fun!".

蒼天幻想ナイツ・オブ・タヌキ(tanuki-第27回世界コンピュータ将棋選手権バージョン)はやねうら王から派生したコンピューター将棋エンジンです。開発コンセプトは「楽に！楽しく！」です。

[やねうら王mini 公式サイト (解説記事、開発者向け情報等)](http://yaneuraou.yaneu.com/YaneuraOu_Mini/)

[やねうら王公式 ](http://yaneuraou.yaneu.com/)

# Using tanuki- with WinBoard 4.8.0
Please follow the steps below.

1. Install WinBoard 4.8.0. The installation path is "C:\WinBoard-4.8.0".
2. Download "tanuki-sdt4-2016-10-09.7z" from https://github.com/nodchip/hakubishin-/releases, and extract to "C:\WinBoard-4.8.0\tanuki-sdt4-2016-10-09".
3. Start "C:\WinBoard-4.8.0\WinBoard\winboard.exe".
4. Check "Advanced options", and set -uxiAdapter {UCI2WB -%variant "%fcp" "%fd"). Please refer #8 (comment) about this step.
5. Add "tanuki-sdt4" with the following settings:
      Engine (.exe or .jar): C:\WinBoard-4.8.0\tanuki-sdt4-2016-10-09\tanuki-sdt4.exe
      command-line parameters: empty
      Special WinBoard options: empty
      directory: empty
      UCCI/USI [uses specified /uxiAdapter]: on
6. Engine > Engine #1 Settings... > Set "Minimum_Thinking_Time" to "0".

The steps will be changed in future versions.

# Q & A
Q. Do you plan to create an all-in-one package with WinBoard and tanuki-?
A. There are no plans for it.

Q. Do you plan to support WB/CECP-protocol in the next tanuki- edition? http://hgm.nubati.net/CECP.html v2 - http://home.hccnet.nl/h.g.muller/engine-intf.html v1
A. There are no plans for it.

Q. Do you plan to add any logos to tanuki-?
A. There are no plans for it.

Q. Why tanuki- uses up all its time with 1 min + 0 sec/move?
A. Start "C:\WinBoard-4.8.0\WinBoard\winboard.exe", check "Advanced options", and set `-uxiAdapter {UCI2WB -%variant "%fcp" "%fd")`.

Q. Why tanuki- uses up all its time in 27-36 moves with 1 min + 0 sec/move?
A. Engine > Engine #1 Settings... > Set "Minimum_Thinking_Time" to "0".
