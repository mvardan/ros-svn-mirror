101 stdcall -noname InitOCHostClass(long)
102 stdcall -noname SHOpenFolderWindow(ptr)
103 stdcall -noname SHOpenNewFrame(ptr ptr long long)
@ stdcall -private DllCanUnloadNow()
105 stdcall -noname SHCreateSavedWindows()
106 stdcall -noname SHCreateFromDesktop(long)
107 stdcall -noname SHExplorerParseCmdLine(wstr)
@ stdcall -private DllGetClassObject(ptr ptr ptr)
@ stdcall -private DllGetVersion(ptr)
@ stdcall -private DllInstall(long wstr)
@ stdcall -private DllRegisterServer()
@ stdcall -private DllUnregisterServer()
118 stdcall -noname UEMRegisterNotify(long long)
120 stdcall -noname SHCreateBandForPidl(ptr ptr long)
121 stdcall -noname SHPidlFromDataObject(ptr ptr long ptr)
122 stdcall -noname IDataObject_GetDeskBandState(long)
123 stdcall -noname SHCreateIETHREADPARAM(long long ptr ptr)
124 stdcall -noname SHCloneIETHREADPARAM(ptr)
125 stdcall -noname SHParseIECommandLine(long long)
126 stdcall -noname SHDestroyIETHREADPARAM(ptr)
127 stdcall -noname SHOnCWMCommandLine(long)
128 stdcall -noname Channel_GetFolderPidl()
129 stdcall -noname ChannelBand_Create(ptr)
130 stdcall -noname Channels_SetBandInfoSFB(ptr)
131 stdcall -noname IUnknown_SetBandInfoSFB(ptr long)
133 stdcall -noname Channel_QuickLaunch()
134 stdcall -noname SHGetNavigateTarget(long long long long)
135 stdcall -noname GetInfoTip(ptr long wstr long)
136 stdcall -noname SHEnumClassesOfCategories(long long long long long)
137 stdcall -noname SHWriteClassesOfCategories(long long long long long long long)
138 stdcall -noname SHIsExplorerBrowser()
