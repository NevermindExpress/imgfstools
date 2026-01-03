//	cecompr_nt_supp.h : supplement header file for cecompr_nt.dll
//	Author: DLL to Lib version 1.42
//	Date: Wednesday, November 30, 2005
//	Description: The declaration of the cecompr_nt.dll's entry-point function.
//	Prototype: BOOL WINAPI xxx_DllMain(HINSTANCE hinstance, DWORD fdwReason, LPVOID lpvReserved);
//	Parameters: 
//		hinstance
//		  Handle to current instance of the application. Use AfxGetInstanceHandle()
//		  to get the instance handle if your project has MFC support.
//		fdwReason
//		  Specifies a flag indicating why the entry-point function is being called.
//		lpvReserved 
//		  Specifies further aspects of DLL initialization and cleanup. Should always
//		  be set to NULL;
//	Comment: Please see the help document for detail information about the entry-point 
//		 function
//	Homepage: http://www.binary-soft.com
//	Technical Support: support@binary-soft.com
/////////////////////////////////////////////////////////////////////

#if !defined(D2L_CECOMPR_NT_SUPP_H__739D7CD7_4535_5A2B_504D_579D0497745B__INCLUDED_)
#define D2L_CECOMPR_NT_SUPP_H__739D7CD7_4535_5A2B_504D_579D0497745B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef __cplusplus
extern "C" {
#endif


#include <windows.h>

/* This is cecompr_nt.dll's entry-point function. You should call it to do necessary
 initialization and finalization. */

BOOL WINAPI CECOMPR_NT_DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);


#ifdef __cplusplus
}
#endif

#endif // !defined(D2L_CECOMPR_NT_SUPP_H__739D7CD7_4535_5A2B_504D_579D0497745B__INCLUDED_)