# About this project

hakubishin (tanuki-sdt4 ver) is a shogi engine (AI player) based on YaneuraOu (https://github.com/yaneurao/YaneuraOu).　　The development concept is "easily and for fun!".

†白美神†(tanuki-第4回電王トーナメントバージョン)はやねうら王(https://github.com/yaneurao/YaneuraOu)から派生したコンピューター将棋エンジンです。開発コンセプトは「楽に！楽しく！」です。

[やねうら王mini 公式サイト (解説記事、開発者向け情報等)](http://yaneuraou.yaneu.com/YaneuraOu_Mini/)

[やねうら王公式 ](http://yaneuraou.yaneu.com/)

# Using tanuki- with WinBoard 4.8.0
Please follow the steps below.

1. Install WinBoard 4.8.0. The installation path is "C:\WinBoard-4.8.0".
2. Download "apery_twig_sdt3.7z" from http://hiraokatakuya.github.io/apery/, and extract to "C:\WinBoard-4.8.0\apery_twig_sdt3".
3. Download "tanuki-2016-09-10.7z" from https://github.com/nodchip/tanuki-/releases, and extract to "C:\WinBoard-4.8.0\apery_twig_sdt3\bin".
4. Start "C:\WinBoard-4.8.0\WinBoard\winboard.exe".
5. Check "Advanced options", and set -uxiAdapter {UCI2WB -%variant "%fcp" "%fd"). Please refer #8 (comment) about this step.
6. Add "tanuki- WCSC26" with the following settings:
      Engine (.exe or .jar): C:\WinBoard-4.8.0\apery_twig_sdt3\bin\tanuki-wcsc26-sse42-gcc-lazy-smp.exe
      command-line parameters: empty
      Special WinBoard options: empty
      directory: empty
      UCCI/USI [uses specified /uxiAdapter]: on
7. Engine > Engine #1 Settings... > Set "Minimum_Thinking_Time" to "0".

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