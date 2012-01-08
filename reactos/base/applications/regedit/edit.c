/*
 * Registry editing UI functions.
 *
 * Copyright (C) 2003 Dimitrie O. Paun
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <regedit.h>

typedef enum _EDIT_MODE
{
    EDIT_MODE_DEC,
    EDIT_MODE_HEX
} EDIT_MODE;


static const TCHAR* editValueName;
static TCHAR* stringValueData;
static PVOID binValueData;
static DWORD dwordValueData;
static PCM_RESOURCE_LIST resourceValueData;
static INT fullResourceIndex = -1;
static DWORD valueDataLen;
static EDIT_MODE dwordEditMode = EDIT_MODE_HEX;

void error(HWND hwnd, INT resId, ...)
{
    va_list ap;
    TCHAR title[256];
    TCHAR errfmt[1024];
    TCHAR errstr[1024];
    HINSTANCE hInstance;

    hInstance = GetModuleHandle(0);

    if (!LoadString(hInstance, IDS_ERROR, title, COUNT_OF(title)))
        _tcscpy(title, _T("Error"));

    if (!LoadString(hInstance, resId, errfmt, COUNT_OF(errfmt)))
        _tcscpy(errfmt, _T("Unknown error string!"));

    va_start(ap, resId);
    _vsntprintf(errstr, COUNT_OF(errstr), errfmt, ap);
    va_end(ap);

    MessageBox(hwnd, errstr, title, MB_OK | MB_ICONERROR);
}

static void error_code_messagebox(HWND hwnd, DWORD error_code)
{
    TCHAR title[256];
    if (!LoadString(hInst, IDS_ERROR, title, COUNT_OF(title)))
        lstrcpy(title, TEXT("Error"));
    ErrorMessageBox(hwnd, title, error_code);
}

void warning(HWND hwnd, INT resId, ...)
{
    va_list ap;
    TCHAR title[256];
    TCHAR errfmt[1024];
    TCHAR errstr[1024];
    HINSTANCE hInstance;

    hInstance = GetModuleHandle(0);

    if (!LoadString(hInstance, IDS_WARNING, title, COUNT_OF(title)))
        _tcscpy(title, _T("Warning"));

    if (!LoadString(hInstance, resId, errfmt, COUNT_OF(errfmt)))
        _tcscpy(errfmt, _T("Unknown error string!"));

    va_start(ap, resId);
    _vsntprintf(errstr, COUNT_OF(errstr), errfmt, ap);
    va_end(ap);

    MessageBox(hwnd, errstr, title, MB_OK | MB_ICONSTOP);
}

INT_PTR CALLBACK modify_string_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TCHAR* valueData;
    HWND hwndValue;
    int len;

    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(editValueName && _tcscmp(editValueName, _T("")))
        {
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, editValueName);
        }
        else
        {
            TCHAR buffer[255];
            LoadString(hInst, IDS_DEFAULT_VALUE_NAME, buffer, sizeof(buffer)/sizeof(TCHAR));
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, buffer);
        }
        SetDlgItemText(hwndDlg, IDC_VALUE_DATA, stringValueData);
        SetFocus(GetDlgItem(hwndDlg, IDC_VALUE_DATA));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
            {
                if ((len = GetWindowTextLength(hwndValue)))
                {
                    if (stringValueData)
                    {
                        if ((valueData = HeapReAlloc(GetProcessHeap(), 0, stringValueData, (len + 1) * sizeof(TCHAR))))
                        {
                            stringValueData = valueData;
                            if (!GetWindowText(hwndValue, stringValueData, len + 1))
                                *stringValueData = 0;
                        }
                    }
                    else
                    {
                        if ((valueData = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(TCHAR))))
                        {
                            stringValueData = valueData;
                            if (!GetWindowText(hwndValue, stringValueData, len + 1))
                                *stringValueData = 0;
                        }
                    }
                }
                else
                {
                    if (stringValueData)
                        *stringValueData = 0;
                }
            }
            EndDialog(hwndDlg, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}


INT_PTR CALLBACK modify_multi_string_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TCHAR* valueData;
    HWND hwndValue;
    int len;

    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(editValueName && _tcscmp(editValueName, _T("")))
        {
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, editValueName);
        }
        else
        {
            TCHAR buffer[255];
            LoadString(hInst, IDS_DEFAULT_VALUE_NAME, buffer, sizeof(buffer)/sizeof(TCHAR));
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, buffer);
        }
        SetDlgItemText(hwndDlg, IDC_VALUE_DATA, stringValueData);
        SetFocus(GetDlgItem(hwndDlg, IDC_VALUE_DATA));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
            {
                if ((len = GetWindowTextLength(hwndValue)))
                {
                    if (stringValueData)
                    {
                        if ((valueData = HeapReAlloc(GetProcessHeap(), 0, stringValueData, (len + 1) * sizeof(TCHAR))))
                        {
                            stringValueData = valueData;
                            if (!GetWindowText(hwndValue, stringValueData, len + 1))
                                *stringValueData = 0;
                        }
                    }
                    else
                    {
                        if ((valueData = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(TCHAR))))
                        {
                            stringValueData = valueData;
                            if (!GetWindowText(hwndValue, stringValueData, len + 1))
                                *stringValueData = 0;
                        }
                    }
                }
                else
                {
                    if (stringValueData)
                        *stringValueData = 0;
                }
            }
            EndDialog(hwndDlg, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}


LRESULT CALLBACK DwordEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldwndproc;

    oldwndproc = (WNDPROC)(LONG_PTR)GetWindowLongPtr(hwnd, GWL_USERDATA);

    switch (uMsg)
    {
    case WM_CHAR:
        if (dwordEditMode == EDIT_MODE_DEC)
        {
            if (isdigit((int) wParam & 0xff) || iscntrl((int) wParam & 0xff))
            {
                break;
            }
            else
            {
                return 0;
            }
        }
        else if (dwordEditMode == EDIT_MODE_HEX)
        {
            if (isxdigit((int) wParam & 0xff) || iscntrl((int) wParam & 0xff))
            {
                break;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            break;
        }
    }

    return CallWindowProc(oldwndproc, hwnd, uMsg, wParam, lParam);
}


INT_PTR CALLBACK modify_dword_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldproc;
    HWND hwndValue;
    TCHAR ValueString[32];
    LPTSTR Remainder;
    DWORD Base;
    DWORD Value = 0;

    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        dwordEditMode = EDIT_MODE_HEX;

        /* subclass the edit control */
        hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA);
        oldproc = (WNDPROC)(LONG_PTR)GetWindowLongPtr(hwndValue, GWL_WNDPROC);
        SetWindowLongPtr(hwndValue, GWL_USERDATA, (DWORD_PTR)oldproc);
        SetWindowLongPtr(hwndValue, GWL_WNDPROC, (DWORD_PTR)DwordEditSubclassProc);

        if(editValueName && _tcscmp(editValueName, _T("")))
        {
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, editValueName);
        }
        else
        {
            TCHAR buffer[255];
            LoadString(hInst, IDS_DEFAULT_VALUE_NAME, buffer, sizeof(buffer)/sizeof(TCHAR));
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, buffer);
        }
        CheckRadioButton (hwndDlg, IDC_FORMAT_HEX, IDC_FORMAT_DEC, IDC_FORMAT_HEX);
        _stprintf (ValueString, _T("%lx"), dwordValueData);
        SetDlgItemText(hwndDlg, IDC_VALUE_DATA, ValueString);
        SetFocus(GetDlgItem(hwndDlg, IDC_VALUE_DATA));
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_FORMAT_HEX:
            if (HIWORD(wParam) == BN_CLICKED && dwordEditMode == EDIT_MODE_DEC)
            {
                dwordEditMode = EDIT_MODE_HEX;
                if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
                {
                    if (GetWindowTextLength(hwndValue))
                    {
                        if (GetWindowText(hwndValue, ValueString, 32))
                        {
                            Value = _tcstoul (ValueString, &Remainder, 10);
                        }
                    }
                }
                _stprintf (ValueString, _T("%lx"), Value);
                SetDlgItemText(hwndDlg, IDC_VALUE_DATA, ValueString);
                return TRUE;
            }
            break;

        case IDC_FORMAT_DEC:
            if (HIWORD(wParam) == BN_CLICKED && dwordEditMode == EDIT_MODE_HEX)
            {
                dwordEditMode = EDIT_MODE_DEC;
                if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
                {
                    if (GetWindowTextLength(hwndValue))
                    {
                        if (GetWindowText(hwndValue, ValueString, 32))
                        {
                            Value = _tcstoul (ValueString, &Remainder, 16);
                        }
                    }
                }
                _stprintf (ValueString, _T("%lu"), Value);
                SetDlgItemText(hwndDlg, IDC_VALUE_DATA, ValueString);
                return TRUE;
            }
            break;

        case IDOK:
            if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
            {
                if (GetWindowTextLength(hwndValue))
                {
                    if (!GetWindowText(hwndValue, ValueString, 32))
                    {
                        EndDialog(hwndDlg, IDCANCEL);
                        return TRUE;
                    }

                    Base = (dwordEditMode == EDIT_MODE_HEX) ? 16 : 10;
                    dwordValueData = _tcstoul (ValueString, &Remainder, Base);
                }
                else
                {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
            }
            EndDialog(hwndDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}


INT_PTR CALLBACK modify_binary_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hwndValue;
    UINT len;

    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(editValueName && _tcscmp(editValueName, _T("")))
        {
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, editValueName);
        }
        else
        {
            TCHAR buffer[255];
            LoadString(hInst, IDS_DEFAULT_VALUE_NAME, buffer, sizeof(buffer)/sizeof(TCHAR));
            SetDlgItemText(hwndDlg, IDC_VALUE_NAME, buffer);
        }
        hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA);
        HexEdit_LoadBuffer(hwndValue, binValueData, valueDataLen);
        /* reset the hex edit control's font */
        SendMessage(hwndValue, WM_SETFONT, 0, 0);
        SetFocus(hwndValue);
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA)))
            {
                len = (UINT) HexEdit_GetBufferSize(hwndValue);
                if (len > 0 && binValueData)
                    binValueData = HeapReAlloc(GetProcessHeap(), 0, binValueData, len);
                else
                    binValueData = HeapAlloc(GetProcessHeap(), 0, len + 1);
                HexEdit_CopyBuffer(hwndValue, binValueData, len);
                valueDataLen = len;
            }
            EndDialog(hwndDlg, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}


static BOOL CreateResourceColumns(HWND hwnd)
{
    TCHAR szText[80];
    RECT rc;
    LV_COLUMN lvC;
    HWND hwndLV;
    INT width;

    /* Create columns. */
    lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvC.pszText = szText;
    lvC.fmt = LVCFMT_LEFT;

    hwndLV = GetDlgItem(hwnd, IDC_DMA_LIST);
    ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT);
    GetClientRect(hwndLV, &rc);

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = (rc.right - rc.left) / 2;
    LoadString(hInst, IDS_DMA_CHANNEL, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    lvC.cx = (rc.right - rc.left) - lvC.cx;
    LoadString(hInst, IDS_DMA_PORT, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 1, &lvC) == -1)
        return FALSE;


    /* Interrupt list */
    hwndLV = GetDlgItem(hwnd, IDC_IRQ_LIST);
    ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT);
    GetClientRect(hwndLV, &rc);
    width = (rc.right - rc.left) / 4;

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = width;
    LoadString(hInst, IDS_INTERRUPT_VECTOR, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    LoadString(hInst, IDS_INTERRUPT_LEVEL, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 1, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 2;
    LoadString(hInst, IDS_INTERRUPT_AFFINITY, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 2, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 3;
    lvC.cx = (rc.right - rc.left) - 3 * width;
    LoadString(hInst, IDS_INTERRUPT_TYPE, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 3, &lvC) == -1)
        return FALSE;


    /* Memory list */
    hwndLV = GetDlgItem(hwnd, IDC_MEMORY_LIST);
    ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT);
    GetClientRect(hwndLV, &rc);
    width = (rc.right - rc.left) / 3;

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = width;
    LoadString(hInst, IDS_MEMORY_ADDRESS, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    LoadString(hInst, IDS_MEMORY_LENGTH, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 1, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 2;
    lvC.cx = (rc.right - rc.left) - 2 * width;
    LoadString(hInst, IDS_MEMORY_ACCESS, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 2, &lvC) == -1)
        return FALSE;


    /* Port list */
    hwndLV = GetDlgItem(hwnd, IDC_PORT_LIST);
    ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT);
    GetClientRect(hwndLV, &rc);
    width = (rc.right - rc.left) / 3;

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = width;
    LoadString(hInst, IDS_PORT_ADDRESS, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    LoadString(hInst, IDS_PORT_LENGTH, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 1, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 2;
    lvC.cx = (rc.right - rc.left) - 2 * width;
    LoadString(hInst, IDS_PORT_ACCESS, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 2, &lvC) == -1)
        return FALSE;

    /* Device specific list */
    hwndLV = GetDlgItem(hwnd, IDC_DEVICE_LIST);
    ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT);
    GetClientRect(hwndLV, &rc);
    width = (rc.right - rc.left) / 3;

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = width;
    LoadString(hInst, IDS_SPECIFIC_RESERVED1, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    LoadString(hInst, IDS_SPECIFIC_RESERVED2, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 1, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 2;
    lvC.cx = (rc.right - rc.left) - 2 * width;
    LoadString(hInst, IDS_SPECIFIC_DATASIZE, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hwndLV, 2, &lvC) == -1)
        return FALSE;

    return TRUE;
}

static VOID
GetInterfaceType(INTERFACE_TYPE InterfaceType,
                 LPTSTR pBuffer,
                 DWORD dwLength)
{
//    LPTSTR lpInterfaceType;

    switch (InterfaceType)
    {
        case InterfaceTypeUndefined:
            LoadString(hInst, IDS_BUS_UNDEFINED, pBuffer, dwLength);
//            lpInterfaceType = _T("Undefined");
            break;
        case Internal:
            LoadString(hInst, IDS_BUS_INTERNAL, pBuffer, dwLength);
//            lpInterfaceType = _T("Internal");
            break;
        case Isa:
            LoadString(hInst, IDS_BUS_ISA, pBuffer, dwLength);
//            lpInterfaceType = _T("Isa");
            break;
        case Eisa:
            LoadString(hInst, IDS_BUS_EISA, pBuffer, dwLength);
//            lpInterfaceType = _T("Eisa");
            break;
        case MicroChannel:
            LoadString(hInst, IDS_BUS_MICROCHANNEL, pBuffer, dwLength);
//            lpInterfaceType = _T("MicroChannel");
            break;
        case TurboChannel:
            LoadString(hInst, IDS_BUS_TURBOCHANNEL, pBuffer, dwLength);
//            lpInterfaceType = _T("TurboChannel");
            break;
        case PCIBus:
            LoadString(hInst, IDS_BUS_PCIBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("PCIBus");
            break;
        case VMEBus:
            LoadString(hInst, IDS_BUS_VMEBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("VMEBus");
            break;
        case NuBus:
            LoadString(hInst, IDS_BUS_NUBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("NuBus");
            break;
        case PCMCIABus:
            LoadString(hInst, IDS_BUS_PCMCIABUS, pBuffer, dwLength);
//            lpInterfaceType = _T("PCMCIABus");
            break;
        case CBus:
            LoadString(hInst, IDS_BUS_CBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("CBus");
            break;
        case MPIBus:
            LoadString(hInst, IDS_BUS_MPIBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("MPIBus");
            break;
        case MPSABus:
            LoadString(hInst, IDS_BUS_MPSABUS, pBuffer, dwLength);
//            lpInterfaceType = _T("MPSABus");
            break;
        case ProcessorInternal:
            LoadString(hInst, IDS_BUS_PROCESSORINTERNAL, pBuffer, dwLength);
//            lpInterfaceType = _T("ProcessorInternal");
            break;
        case InternalPowerBus:
            LoadString(hInst, IDS_BUS_INTERNALPOWERBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("InternalPowerBus");
            break;
        case PNPISABus:
            LoadString(hInst, IDS_BUS_PNPISABUS, pBuffer, dwLength);
//            lpInterfaceType = _T("PNPISABus");
            break;
        case PNPBus:
            LoadString(hInst, IDS_BUS_PNPBUS, pBuffer, dwLength);
//            lpInterfaceType = _T("PNPBus");
            break;
        default:
            LoadString(hInst, IDS_BUS_UNKNOWNTYPE, pBuffer, dwLength);
//            lpInterfaceType = _T("Unknown interface type");
            break;
    }

//    _tcscpy(pBuffer, lpInterfaceType);
}


static VOID
ParseResources(HWND hwnd)
{
    PCM_FULL_RESOURCE_DESCRIPTOR pFullDescriptor;
    PCM_PARTIAL_RESOURCE_LIST pPartialResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;
    ULONG i;
    HWND hwndLV;

    TCHAR buffer[80];
    LVITEM item;
    INT iItem;

    pFullDescriptor = &resourceValueData->List[fullResourceIndex];
    pPartialResourceList = &pFullDescriptor->PartialResourceList;

    /* Interface type */
    GetInterfaceType(pFullDescriptor->InterfaceType, buffer, 80);
    SetDlgItemText(hwnd, IDC_INTERFACETYPE, buffer);

    /* Busnumber */
    SetDlgItemInt(hwnd, IDC_BUSNUMBER, (UINT)pFullDescriptor->BusNumber, FALSE);

    /* Version */
    SetDlgItemInt(hwnd, IDC_VERSION, (UINT)pPartialResourceList->Version, FALSE);

    /* Revision */
    SetDlgItemInt(hwnd, IDC_REVISION, (UINT)pPartialResourceList->Revision, FALSE);

    for (i = 0; i < pPartialResourceList->Count; i++)
    {
        pDescriptor = &pPartialResourceList->PartialDescriptors[i];

        switch (pDescriptor->Type)
        {
            case CmResourceTypePort:
                hwndLV = GetDlgItem(hwnd, IDC_PORT_LIST);

#ifdef _M_AMD64
                wsprintf(buffer, _T("0x%16I64x"), pDescriptor->u.Port.Start.QuadPart);
#else
                wsprintf(buffer, _T("0x%08lx"), pDescriptor->u.Port.Start.u.LowPart);
#endif

                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.iItem = 1000;
                item.iSubItem = 0;
                item.state = 0;
                item.stateMask = 0;
                item.pszText = buffer;
                item.cchTextMax = (int)_tcslen(item.pszText);
                item.lParam = (LPARAM)pDescriptor;

                iItem = ListView_InsertItem(hwndLV, &item);
                if (iItem != -1)
                {
                    wsprintf(buffer, _T("0x%lx"), pDescriptor->u.Port.Length);
                    ListView_SetItemText(hwndLV, iItem, 1, buffer);

                    if (pDescriptor->Flags & CM_RESOURCE_PORT_IO)
                        LoadString(hInst, IDS_PORT_PORT_IO, buffer, sizeof(buffer)/sizeof(TCHAR));
                    else
                        LoadString(hInst, IDS_PORT_MEMORY_IO, buffer, sizeof(buffer)/sizeof(TCHAR));
                    ListView_SetItemText(hwndLV, iItem, 2, buffer);
                }
                break;

            case CmResourceTypeInterrupt:
                hwndLV = GetDlgItem(hwnd, IDC_IRQ_LIST);

                wsprintf(buffer, _T("%lu"), pDescriptor->u.Interrupt.Vector);

                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.iItem = 1000;
                item.iSubItem = 0;
                item.state = 0;
                item.stateMask = 0;
                item.pszText = buffer;
                item.cchTextMax = (int)_tcslen(item.pszText);
                item.lParam = (LPARAM)pDescriptor;

                iItem = ListView_InsertItem(hwndLV, &item);
                if (iItem != -1)
                {
                    wsprintf(buffer, _T("%lu"), pDescriptor->u.Interrupt.Level);
                    ListView_SetItemText(hwndLV, iItem, 1, buffer);

                    wsprintf(buffer, _T("0x%08lx"), pDescriptor->u.Interrupt.Affinity);
                    ListView_SetItemText(hwndLV, iItem, 2, buffer);

                    if (pDescriptor->Flags & CM_RESOURCE_INTERRUPT_LATCHED)
                        LoadString(hInst, IDS_INTERRUPT_EDGE_SENSITIVE, buffer, sizeof(buffer)/sizeof(TCHAR));
                    else
                        LoadString(hInst, IDS_INTERRUPT_LEVEL_SENSITIVE, buffer, sizeof(buffer)/sizeof(TCHAR));

                    ListView_SetItemText(hwndLV, iItem, 3, buffer);
                }
                break;

            case CmResourceTypeMemory:
                hwndLV = GetDlgItem(hwnd, IDC_MEMORY_LIST);

#ifdef _M_AMD64
                wsprintf(buffer, _T("0x%16I64x"), pDescriptor->u.Memory.Start.QuadPart);
#else
                wsprintf(buffer, _T("0x%08lx"), pDescriptor->u.Memory.Start.u.LowPart);
#endif

                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.iItem = 1000;
                item.iSubItem = 0;
                item.state = 0;
                item.stateMask = 0;
                item.pszText = buffer;
                item.cchTextMax = (int)_tcslen(item.pszText);
                item.lParam = (LPARAM)pDescriptor;

                iItem = ListView_InsertItem(hwndLV, &item);
                if (iItem != -1)
                {
                    wsprintf(buffer, _T("0x%lx"), pDescriptor->u.Memory.Length);
                    ListView_SetItemText(hwndLV, iItem, 1, buffer);

                    switch (pDescriptor->Flags & (CM_RESOURCE_MEMORY_READ_ONLY | CM_RESOURCE_MEMORY_WRITE_ONLY))
                    {
                        case CM_RESOURCE_MEMORY_READ_ONLY:
                            LoadString(hInst, IDS_MEMORY_READ_ONLY, buffer, sizeof(buffer)/sizeof(TCHAR));
                            break;

                        case CM_RESOURCE_MEMORY_WRITE_ONLY:
                            LoadString(hInst, IDS_MEMORY_WRITE_ONLY, buffer, sizeof(buffer)/sizeof(TCHAR));
                            break;

                        default:
                            LoadString(hInst, IDS_MEMORY_READ_WRITE, buffer, sizeof(buffer)/sizeof(TCHAR));
                            break;
                    }

                    ListView_SetItemText(hwndLV, iItem, 2, buffer);
                }
                break;

            case CmResourceTypeDma:
                hwndLV = GetDlgItem(hwnd, IDC_DMA_LIST);

                wsprintf(buffer, _T("%lu"), pDescriptor->u.Dma.Channel);

                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.iItem = 1000;
                item.iSubItem = 0;
                item.state = 0;
                item.stateMask = 0;
                item.pszText = buffer;
                item.cchTextMax = (int)_tcslen(item.pszText);
                item.lParam = (LPARAM)pDescriptor;

                iItem = ListView_InsertItem(hwndLV, &item);
                if (iItem != -1)
                {
                    wsprintf(buffer, _T("%lu"), pDescriptor->u.Dma.Port);
                    ListView_SetItemText(hwndLV, iItem, 1, buffer);
                }
                break;

            case CmResourceTypeDeviceSpecific:
                hwndLV = GetDlgItem(hwnd, IDC_DEVICE_LIST);

                wsprintf(buffer, _T("0x%08lx"), pDescriptor->u.DeviceSpecificData.Reserved1);

                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.iItem = 1000;
                item.iSubItem = 0;
                item.state = 0;
                item.stateMask = 0;
                item.pszText = buffer;
                item.cchTextMax = (int)_tcslen(item.pszText);
                item.lParam = (LPARAM)pDescriptor;

                iItem = ListView_InsertItem(hwndLV, &item);
                if (iItem != -1)
                {
                    wsprintf(buffer, _T("0x%08lx"), pDescriptor->u.DeviceSpecificData.Reserved2);
                    ListView_SetItemText(hwndLV, iItem, 1, buffer);

                    wsprintf(buffer, _T("0x%lx"), pDescriptor->u.DeviceSpecificData.DataSize);
                    ListView_SetItemText(hwndLV, iItem, 2, buffer);
                }
                break;
        }
    }
}


static BOOL
OnResourceNotify(HWND hwndDlg, NMHDR *phdr)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)phdr;

    switch (phdr->idFrom)
    {
        case IDC_PORT_LIST:
        case IDC_MEMORY_LIST:
        case IDC_DMA_LIST:
        case IDC_IRQ_LIST:
        case IDC_DEVICE_LIST:
            switch(phdr->code)
            {
                case NM_CLICK:
                    if (lpnmlv->iItem != -1)
                    {
                        PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;
                        LVITEM item;

                        item.mask = LVIF_PARAM;
                        item.iItem = lpnmlv->iItem;
                        item.iSubItem = 0;

                        if (ListView_GetItem(phdr->hwndFrom, &item))
                        {
                            pDescriptor = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)item.lParam;

                            EnableWindow(GetDlgItem(hwndDlg, IDC_UNDETERMINED),
                                         (pDescriptor->ShareDisposition == CmResourceShareUndetermined));

                            EnableWindow(GetDlgItem(hwndDlg, IDC_SHARED),
                                         (pDescriptor->ShareDisposition == CmResourceShareShared));

                            EnableWindow(GetDlgItem(hwndDlg, IDC_DEVICE_EXCLUSIVE),
                                         (pDescriptor->ShareDisposition == CmResourceShareDeviceExclusive));

                            EnableWindow(GetDlgItem(hwndDlg, IDC_DRIVER_EXCLUSIVE),
                                         (pDescriptor->ShareDisposition == CmResourceShareDriverExclusive));
                        }
                    }
                    else
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_UNDETERMINED), FALSE);
                        EnableWindow(GetDlgItem(hwndDlg, IDC_SHARED), FALSE);
                        EnableWindow(GetDlgItem(hwndDlg, IDC_DEVICE_EXCLUSIVE), FALSE);
                        EnableWindow(GetDlgItem(hwndDlg, IDC_DRIVER_EXCLUSIVE), FALSE);
                    }
                    break;
            }
            break;
    }

    return FALSE;
}


static INT_PTR CALLBACK modify_resource_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        CreateResourceColumns(hwndDlg);
        ParseResources(hwndDlg);
        return FALSE;

    case WM_NOTIFY:
        return OnResourceNotify(hwndDlg, (NMHDR *)lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            EndDialog(hwndDlg, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL CreateResourceListColumns(HWND hWndListView)
{
    TCHAR szText[80];
    RECT rc;
    LV_COLUMN lvC;

    ListView_SetExtendedListViewStyle(hWndListView, LVS_EX_FULLROWSELECT);

    GetClientRect(hWndListView, &rc);

    /* Create columns. */
    lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvC.pszText = szText;
    lvC.fmt = LVCFMT_LEFT;

    /* Load the column labels from the resource file. */
    lvC.iSubItem = 0;
    lvC.cx = (rc.right - rc.left) / 2;
    LoadString(hInst, IDS_BUSNUMBER, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hWndListView, 0, &lvC) == -1)
        return FALSE;

    lvC.iSubItem = 1;
    lvC.cx = (rc.right - rc.left) - lvC.cx;
    LoadString(hInst, IDS_INTERFACE, szText, sizeof(szText)/sizeof(TCHAR));
    if (ListView_InsertColumn(hWndListView, 1, &lvC) == -1)
        return FALSE;

    return TRUE;
}

static VOID AddFullResourcesToList(HWND hwnd)
{
    PCM_FULL_RESOURCE_DESCRIPTOR pFullDescriptor;
    TCHAR buffer[80];
    LVITEM item;
    ULONG i;
    INT iItem;

    for (i = 0; i < resourceValueData->Count; i++)
    {
        pFullDescriptor = &resourceValueData->List[i];

        wsprintf(buffer, _T("%lu"), pFullDescriptor->BusNumber);

        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.state = 0;
        item.stateMask = 0;
        item.pszText = buffer;
        item.cchTextMax = (int)_tcslen(item.pszText);

        iItem = ListView_InsertItem(hwnd, &item);
        if (iItem != -1)
        {
            GetInterfaceType(pFullDescriptor->InterfaceType, buffer, 80);
            ListView_SetItemText(hwnd, iItem, 1, buffer);
        }
    }
}

static BOOL
OnResourceListNotify(HWND hwndDlg, NMHDR *phdr)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)phdr;

    switch (phdr->idFrom)
    {
        case IDC_RESOURCE_LIST:
            switch(phdr->code)
            {
                case NM_CLICK:
                    fullResourceIndex = lpnmlv->iItem;
                    EnableWindow(GetDlgItem(hwndDlg, IDC_SHOW_RESOURCE), (lpnmlv->iItem != -1));
                    break;

                case NM_DBLCLK:
                    if (lpnmlv->iItem != -1)
                    {
                        fullResourceIndex = lpnmlv->iItem;
                        DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_RESOURCE), hwndDlg, modify_resource_dlgproc);
                    }
                    break;
            }
            break;
    }

    return FALSE;
}


static INT_PTR CALLBACK modify_resource_list_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch(uMsg)
    {
    case WM_INITDIALOG:
        CreateResourceListColumns(GetDlgItem(hwndDlg, IDC_RESOURCE_LIST));
        AddFullResourcesToList(GetDlgItem(hwndDlg, IDC_RESOURCE_LIST));
        return FALSE;

    case WM_NOTIFY:
        return OnResourceListNotify(hwndDlg, (NMHDR *)lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SHOW_RESOURCE:
            if (fullResourceIndex != -1)
                DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_RESOURCE), hwndDlg, modify_resource_dlgproc);
            break;
        case IDOK:
            EndDialog(hwndDlg, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}


BOOL ModifyValue(HWND hwnd, HKEY hKey, LPCTSTR valueName, BOOL EditBin)
{
    DWORD type;
    LONG lRet;
    BOOL result = FALSE;

    if (!hKey)
        return FALSE;

    editValueName = valueName;

    lRet = RegQueryValueEx(hKey, valueName, 0, &type, 0, &valueDataLen);
    if (lRet != ERROR_SUCCESS && (!_tcscmp(valueName, _T("")) || valueName == NULL))
    {
        lRet = ERROR_SUCCESS; /* Allow editing of (Default) values which don't exist */
        type = REG_SZ;
        valueDataLen = 0;
        stringValueData = NULL;
        binValueData = NULL;
    }

    if (lRet != ERROR_SUCCESS)
    {
        error(hwnd, IDS_BAD_VALUE, valueName);
        goto done;
    }

    if (EditBin == FALSE && ((type == REG_SZ) || (type == REG_EXPAND_SZ)))
    {
        if (valueDataLen > 0)
        {
            if (!(stringValueData = HeapAlloc(GetProcessHeap(), 0, valueDataLen)))
            {
                error(hwnd, IDS_TOO_BIG_VALUE, valueDataLen);
                goto done;
            }
            lRet = RegQueryValueEx(hKey, valueName, 0, 0, (LPBYTE)stringValueData, &valueDataLen);
            if (lRet != ERROR_SUCCESS)
            {
                error(hwnd, IDS_BAD_VALUE, valueName);
                goto done;
            }
        }
        else
        {
            stringValueData = NULL;
        }

        if (DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_STRING), hwnd, modify_string_dlgproc) == IDOK)
        {
            if (stringValueData)
            {
                lRet = RegSetValueEx(hKey, valueName, 0, type, (LPBYTE)stringValueData, (DWORD) (_tcslen(stringValueData) + 1) * sizeof(TCHAR));
            }
            else
            {
                lRet = RegSetValueEx(hKey, valueName, 0, type, NULL, 0);
            }
            if (lRet == ERROR_SUCCESS)
                result = TRUE;
        }
    }
    else if (EditBin == FALSE && type == REG_MULTI_SZ)
    {
        if (valueDataLen > 0)
        {
            size_t llen, listlen, nl_len;
            LPTSTR src, lines = NULL;

            if (!(stringValueData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, valueDataLen + sizeof(TCHAR))))
            {
                error(hwnd, IDS_TOO_BIG_VALUE, valueDataLen);
                goto done;
            }
            lRet = RegQueryValueEx(hKey, valueName, 0, 0, (LPBYTE)stringValueData, &valueDataLen);
            if (lRet != ERROR_SUCCESS)
            {
                error(hwnd, IDS_BAD_VALUE, valueName);
                goto done;
            }

            /* convert \0 to \r\n */
            src = stringValueData;
            nl_len = _tcslen(_T("\r\n")) * sizeof(TCHAR);
            listlen = sizeof(TCHAR);
            lines = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, listlen + sizeof(TCHAR));
            while(*src != _T('\0'))
            {
                llen = _tcslen(src);
                if(llen == 0)
                    break;
                listlen += (llen * sizeof(TCHAR)) + nl_len;
                lines = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, lines, listlen);
                _tcscat(lines, src);
                _tcscat(lines, _T("\r\n"));
                src += llen + 1;
            }
            HeapFree(GetProcessHeap(), 0, stringValueData);
            stringValueData = lines;
        }
        else
        {
            stringValueData = NULL;
        }

        if (DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_MULTI_STRING), hwnd, modify_multi_string_dlgproc) == IDOK)
        {
            if (stringValueData)
            {
                /* convert \r\n to \0 */
                BOOL EmptyLines = FALSE;
                LPTSTR src, lines, nl;
                size_t linechars, buflen, c_nl, dest;

                src = stringValueData;
                buflen = sizeof(TCHAR);
                lines = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buflen + sizeof(TCHAR));
                c_nl = _tcslen(_T("\r\n"));
                dest = 0;
                while(*src != _T('\0'))
                {
                    if((nl = _tcsstr(src, _T("\r\n"))))
                    {
                        linechars = nl - src;
                        if(nl == src)
                        {
                            EmptyLines = TRUE;
                            src = nl + c_nl;
                            continue;
                        }
                    }
                    else
                    {
                        linechars = _tcslen(src);
                    }
                    if(linechars > 0)
                    {
                        buflen += ((linechars + 1) * sizeof(TCHAR));
                        lines = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, lines, buflen);
                        memcpy((lines + dest), src, linechars * sizeof(TCHAR));
                        dest += linechars;
                        lines[dest++] = _T('\0');
                    }
                    else
                    {
                        EmptyLines = TRUE;
                    }
                    src += linechars + (nl != NULL ? c_nl : 0);
                }
                lines[++dest] = _T('\0');

                if(EmptyLines)
                {
                    warning(hwnd, IDS_MULTI_SZ_EMPTY_STRING);
                }

                lRet = RegSetValueEx(hKey, valueName, 0, type, (LPBYTE)lines, (DWORD) buflen);
                HeapFree(GetProcessHeap(), 0, lines);
            }
            else
            {
                lRet = RegSetValueEx(hKey, valueName, 0, type, NULL, 0);
            }
            if (lRet == ERROR_SUCCESS)
                result = TRUE;
        }
    }
    else if (EditBin == FALSE && type == REG_DWORD)
    {
        lRet = RegQueryValueEx(hKey, valueName, 0, 0, (LPBYTE)&dwordValueData, &valueDataLen);
        if (lRet != ERROR_SUCCESS)
        {
            error(hwnd, IDS_BAD_VALUE, valueName);
            goto done;
        }

        if (DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_DWORD), hwnd, modify_dword_dlgproc) == IDOK)
        {
            lRet = RegSetValueEx(hKey, valueName, 0, type, (LPBYTE)&dwordValueData, sizeof(DWORD));
            if (lRet == ERROR_SUCCESS)
                result = TRUE;
        }
    }
    else if (EditBin == FALSE && type == REG_RESOURCE_LIST)
    {
        if (valueDataLen > 0)
        {
            resourceValueData = HeapAlloc(GetProcessHeap(), 0, valueDataLen);
            if (resourceValueData == NULL)
            {
                error(hwnd, IDS_TOO_BIG_VALUE, valueDataLen);
                goto done;
            }

            lRet = RegQueryValueEx(hKey, valueName, 0, 0, (LPBYTE)resourceValueData, &valueDataLen);
            if (lRet != ERROR_SUCCESS)
            {
                error(hwnd, IDS_BAD_VALUE, valueName);
                goto done;
            }
        }
        else
        {
            resourceValueData = NULL;
        }

        if (DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_RESOURCE_LIST), hwnd, modify_resource_list_dlgproc) == IDOK)
        {
        }
    }
    else if (EditBin == TRUE || type == REG_NONE || type == REG_BINARY)
    {
#ifndef UNICODE
        LPWSTR u_valuename;
        int len_vname = lstrlen(valueName);

        if(len_vname > 0)
        {
            if(!(u_valuename = HeapAlloc(GetProcessHeap(), 0, (len_vname + 1) * sizeof(WCHAR))))
            {
                error(hwnd, IDS_TOO_BIG_VALUE, len_vname);
                goto done;
            }
            /* convert the ansi value name to an unicode string */
            MultiByteToWideChar(CP_ACP, 0, valueName, -1, u_valuename, len_vname + 1);
            valueDataLen *= sizeof(WCHAR);
        }
        else
            u_valuename = L"";
#endif
        if(valueDataLen > 0)
        {
            if(!(binValueData = HeapAlloc(GetProcessHeap(), 0, valueDataLen + 1)))
            {
                error(hwnd, IDS_TOO_BIG_VALUE, valueDataLen);
                goto done;
            }

            /* force to use the unicode version, so editing strings in binary mode is correct */
            lRet = RegQueryValueExW(hKey,
#ifndef UNICODE
                                    u_valuename,
#else
                                    valueName,
#endif
                                    0, 0, (LPBYTE)binValueData, &valueDataLen);
            if (lRet != ERROR_SUCCESS)
            {
                HeapFree(GetProcessHeap(), 0, binValueData);
#ifndef UNICODE
                if(len_vname > 0)
                    HeapFree(GetProcessHeap(), 0, u_valuename);
#endif
                error(hwnd, IDS_BAD_VALUE, valueName);
                goto done;
            }
        }
        else
        {
            binValueData = NULL;
        }

        if (DialogBox(0, MAKEINTRESOURCE(IDD_EDIT_BIN_DATA), hwnd, modify_binary_dlgproc) == IDOK)
        {
            /* force to use the unicode version, so editing strings in binary mode is correct */
            lRet = RegSetValueExW(hKey,
#ifndef UNICODE
                                  u_valuename,
#else
                                  valueName,
#endif
                                  0, type, (LPBYTE)binValueData, valueDataLen);
            if (lRet == ERROR_SUCCESS)
                result = TRUE;
        }
        if(binValueData != NULL)
            HeapFree(GetProcessHeap(), 0, binValueData);
#ifndef UNICODE
        if(len_vname > 0)
            HeapFree(GetProcessHeap(), 0, u_valuename);
#endif
    }
    else
    {
        error(hwnd, IDS_UNSUPPORTED_TYPE, type);
    }

done:
    if (resourceValueData)
        HeapFree(GetProcessHeap(), 0, resourceValueData);
    resourceValueData = NULL;

    if (stringValueData)
        HeapFree(GetProcessHeap(), 0, stringValueData);
    stringValueData = NULL;

    return result;
}

static LONG CopyKey(HKEY hDestKey, LPCTSTR lpDestSubKey, HKEY hSrcKey, LPCTSTR lpSrcSubKey)
{
    LONG lResult;
    DWORD dwDisposition;
    HKEY hDestSubKey = NULL;
    HKEY hSrcSubKey = NULL;
    DWORD dwIndex, dwType, cbName, cbData;
    TCHAR szSubKey[256];
    TCHAR szValueName[256];
    BYTE szValueData[512];

    FILETIME ft;

    /* open the source subkey, if specified */
    if (lpSrcSubKey)
    {
        lResult = RegOpenKeyEx(hSrcKey, lpSrcSubKey, 0, KEY_ALL_ACCESS, &hSrcSubKey);
        if (lResult)
            goto done;
        hSrcKey = hSrcSubKey;
    }

    /* create the destination subkey */
    lResult = RegCreateKeyEx(hDestKey, lpDestSubKey, 0, NULL, 0, KEY_WRITE, NULL,
                             &hDestSubKey, &dwDisposition);
    if (lResult)
        goto done;

    /* copy all subkeys */
    dwIndex = 0;
    do
    {
        cbName = sizeof(szSubKey) / sizeof(szSubKey[0]);
        lResult = RegEnumKeyEx(hSrcKey, dwIndex++, szSubKey, &cbName, NULL, NULL, NULL, &ft);
        if (lResult == ERROR_SUCCESS)
        {
            lResult = CopyKey(hDestSubKey, szSubKey, hSrcKey, szSubKey);
            if (lResult)
                goto done;
        }
    }
    while(lResult == ERROR_SUCCESS);

    /* copy all subvalues */
    dwIndex = 0;
    do
    {
        cbName = sizeof(szValueName) / sizeof(szValueName[0]);
        cbData = sizeof(szValueData) / sizeof(szValueData[0]);
        lResult = RegEnumValue(hSrcKey, dwIndex++, szValueName, &cbName, NULL, &dwType, szValueData, &cbData);
        if (lResult == ERROR_SUCCESS)
        {
            lResult = RegSetValueEx(hDestSubKey, szValueName, 0, dwType, szValueData, cbData);
            if (lResult)
                goto done;
        }
    }
    while(lResult == ERROR_SUCCESS);

    lResult = ERROR_SUCCESS;

done:
    if (hSrcSubKey)
        RegCloseKey(hSrcSubKey);
    if (hDestSubKey)
        RegCloseKey(hDestSubKey);
    if (lResult != ERROR_SUCCESS)
        SHDeleteKey(hDestKey, lpDestSubKey);
    return lResult;
}

static LONG MoveKey(HKEY hDestKey, LPCTSTR lpDestSubKey, HKEY hSrcKey, LPCTSTR lpSrcSubKey)
{
    LONG lResult;

    if (!lpSrcSubKey)
        return ERROR_INVALID_FUNCTION;

    lResult = CopyKey(hDestKey, lpDestSubKey, hSrcKey, lpSrcSubKey);
    if (lResult == ERROR_SUCCESS)
        SHDeleteKey(hSrcKey, lpSrcSubKey);

    return lResult;
}

BOOL DeleteKey(HWND hwnd, HKEY hKeyRoot, LPCTSTR keyPath)
{
    TCHAR msg[128], caption[128];
    BOOL result = FALSE;
    LONG lRet;
    HKEY hKey;

    lRet = RegOpenKeyEx(hKeyRoot, keyPath, 0, KEY_READ|KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        error_code_messagebox(hwnd, lRet);
        return FALSE;
    }

    LoadString(hInst, IDS_QUERY_DELETE_KEY_CONFIRM, caption, sizeof(caption)/sizeof(TCHAR));
    LoadString(hInst, IDS_QUERY_DELETE_KEY_ONE, msg, sizeof(msg)/sizeof(TCHAR));

    if (MessageBox(g_pChildWnd->hWnd, msg, caption, MB_ICONQUESTION | MB_YESNO) != IDYES)
        goto done;

    lRet = SHDeleteKey(hKeyRoot, keyPath);
    if (lRet != ERROR_SUCCESS)
    {
        error(hwnd, IDS_BAD_KEY, keyPath);
        goto done;
    }
    result = TRUE;

done:
    RegCloseKey(hKey);
    return result;
}

LONG RenameKey(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpNewName)
{
    LPCTSTR s;
    LPTSTR lpNewSubKey = NULL;
    LONG Ret = 0;

    if (!lpSubKey)
        return Ret;

    s = _tcsrchr(lpSubKey, _T('\\'));
    if (s)
    {
        s++;
        lpNewSubKey = (LPTSTR) HeapAlloc(GetProcessHeap(), 0, (s - lpSubKey + _tcslen(lpNewName) + 1) * sizeof(TCHAR));
        if (lpNewSubKey != NULL)
        {
            memcpy(lpNewSubKey, lpSubKey, (s - lpSubKey) * sizeof(TCHAR));
            lstrcpy(lpNewSubKey + (s - lpSubKey), lpNewName);
            lpNewName = lpNewSubKey;
        }
        else
            return ERROR_NOT_ENOUGH_MEMORY;
    }

    Ret = MoveKey(hKey, lpNewName, hKey, lpSubKey);

    if (lpNewSubKey)
    {
        HeapFree(GetProcessHeap(), 0, lpNewSubKey);
    }
    return Ret;
}

LONG RenameValue(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpDestValue, LPCTSTR lpSrcValue)
{
    LONG lResult;
    HKEY hSubKey = NULL;
    DWORD dwType, cbData;
    BYTE data[512];

    if (lpSubKey)
    {
        lResult = RegOpenKey(hKey, lpSubKey, &hSubKey);
        if (lResult != ERROR_SUCCESS)
            goto done;
        hKey = hSubKey;
    }

    cbData = sizeof(data);
    lResult = RegQueryValueEx(hKey, lpSrcValue, NULL, &dwType, data, &cbData);
    if (lResult != ERROR_SUCCESS)
        goto done;

    lResult = RegSetValueEx(hKey, lpDestValue, 0, dwType, data, cbData);
    if (lResult != ERROR_SUCCESS)
        goto done;

    RegDeleteValue(hKey, lpSrcValue);

done:
    if (hSubKey)
        RegCloseKey(hSubKey);
    return lResult;
}

LONG QueryStringValue(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValueName, LPTSTR pszBuffer, DWORD dwBufferLen)
{
    LONG lResult;
    HKEY hSubKey = NULL;
    DWORD cbData, dwType;

    if (lpSubKey)
    {
        lResult = RegOpenKey(hKey, lpSubKey, &hSubKey);
        if (lResult != ERROR_SUCCESS)
            goto done;
        hKey = hSubKey;
    }

    cbData = (dwBufferLen - 1) * sizeof(*pszBuffer);
    lResult = RegQueryValueEx(hKey, lpValueName, NULL, &dwType, (LPBYTE) pszBuffer, &cbData);
    if (lResult != ERROR_SUCCESS)
        goto done;
    if (dwType != REG_SZ)
    {
        lResult = -1;
        goto done;
    }

    pszBuffer[cbData / sizeof(*pszBuffer)] = _T('\0');

done:
    if (lResult != ERROR_SUCCESS)
        pszBuffer[0] = _T('\0');
    if (hSubKey)
        RegCloseKey(hSubKey);
    return lResult;
}

BOOL GetKeyName(LPTSTR pszDest, size_t iDestLength, HKEY hRootKey, LPCTSTR lpSubKey)
{
    LPCTSTR pszRootKey;

    if (hRootKey == HKEY_CLASSES_ROOT)
        pszRootKey = TEXT("HKEY_CLASSES_ROOT");
    else if (hRootKey == HKEY_CURRENT_USER)
        pszRootKey = TEXT("HKEY_CURRENT_USER");
    else if (hRootKey == HKEY_LOCAL_MACHINE)
        pszRootKey = TEXT("HKEY_LOCAL_MACHINE");
    else if (hRootKey == HKEY_USERS)
        pszRootKey = TEXT("HKEY_USERS");
    else if (hRootKey == HKEY_CURRENT_CONFIG)
        pszRootKey = TEXT("HKEY_CURRENT_CONFIG");
    else if (hRootKey == HKEY_DYN_DATA)
        pszRootKey = TEXT("HKEY_DYN_DATA");
    else
        return FALSE;

    if (lpSubKey[0])
        _sntprintf(pszDest, iDestLength, TEXT("%s\\%s"), pszRootKey, lpSubKey);
    else
        _sntprintf(pszDest, iDestLength, TEXT("%s"), pszRootKey);
    return TRUE;
}
