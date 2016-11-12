SCCISIM ver 1.04

■これは何？
　SCCI音源シミュレータです。
音源ログ出力機能があり、sccisim.iniを変更することで有効になります。

現在、OPM/OPLL/OPNA/SSG/OPL3のシミュレーションが可能です。

コードとしてaslplug(nezplug++改造版)と、fmgen.cppを使用しています。

■使用方法
　scci.dllが必要なアプリケーションと同じフォルダにscci.dllを置いてください。
sccisim.iniは無ければ自動で生成されます。

■sccisim.ini設定一例
debuglog : 1でコンソール表示内容をログファイル出力
soundlog : 1で音源ログ出力。
use_fmgen : 1でFMGEN使用。
pcmlog : 1でPCMファイル出力(raw)。
nlgmode : 1でNLGで音源ログを出力
freq : シミュレータ再生周波数
maxchip : 払い出し可能な最大チップ数
chiptype0 - chiptype7 : チップの種類
chipclock0 - chipclock7 : チップクロック
chipvol0 - chipvol7 : チップ音量

■チップの種類
現在はSC_CHIP_TYPEと同じ数字で設定。
主なSC_CHIP_TYPE
YM2608 : 1
YM2151 : 2
YM2151 : 2
YM2413 : 10

■履歴
20161112
初回起動時に設定が反映されないバグを修正。
PCM録音モード、デバイス毎ボリュームなどの追加。

20161109
設定周りを少し変更した。

20161109
SoundInterface周りをひとまず実装した。
SoundChipの生成方法を変更した。

20161106
チップのクロックを返すようにした。(OPNAが使用可能になった)

20161105
ドキュメント更新

20161104
初期バージョン

