; Removes what Blitzen adds outside its folder: the Explorer menu entry "Ask Blitzen about this file".
!macro NSIS_HOOK_POSTUNINSTALL
  DeleteRegKey HKCU "Software\Classes\*\shell\BlitzenAsk"
!macroend
