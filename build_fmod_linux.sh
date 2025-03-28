wine armlibgen.exe lib.emd --entry-src libevL.s 
wine psp2snc.exe libevL.s -c -o libevL.o
wine psp2ld.exe --oformat prx --prx-no-runtime-support libevL.o crt0_module_snc.o -lSceLibKernel_stub -lSceAudio_stub -lSceAudioIn_stub -lSceSysmodule_stub -lSceAudiodec_stub -lSceThreadmgr_stub -lc_stub -lstdc++_stub -lm_stub -lSceNgs_stub_weak -lSceNet_stub_weak -lsnc -lSceFpu -lfmodexL -lfmodeventL -o libEVL3.o
wine vdsuite-pubprx.exe libEVL3.o fmodex_debug.suprx --compress
wine armlibgen.exe --dump lib.emd --stub-archive
wine vdsuite-libgen.exe --output-kind VitasdkStub lib_vitasdk.yml vitasdk_stub2
