.set noreorder
#include "pspimport.s"
IMPORT_START "SystemCtrlForUser",0x40090000
IMPORT_FUNC "SystemCtrlForUser",0x1090A2E1,sctrlHENGetVersion
IMPORT_FUNC "SystemCtrlForUser",0x5328B431,sctrlHENGetMinorVersion
IMPORT_FUNC "SystemCtrlForUser",0xB00B1E55,sctrlArkGetConfig
IMPORT_START "KUBridge",0x40090000
IMPORT_FUNC "KUBridge",0x24331850,kuKernelGetModel
