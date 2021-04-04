/*
 * PROJECT:     ReactOS System Control Panel Applet
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        dll/cpl/sysdm/environment.c
 * PURPOSE:     Environment variable settings
 * COPYRIGHT:   Copyright Eric Kohl
 *              Copyright Arnav Bhatt <arnavbhatt288@gmail.com>
 *
 */

#include "precomp.h"
#include <commdlg.h>
#include <string.h>
#include <strsafe.h>

typedef struct _VARIABLE_DATA
{
    DWORD dwType;
    LPTSTR lpName;
    LPTSTR lpRawValue;
    LPTSTR lpCookedValue;
} VARIABLE_DATA, *PVARIABLE_DATA;

typedef struct _DIALOG_DATA
{
    BOOL bIsFriendlyUI;
    DWORD dwSelectedValueIndex;
    DWORD dwTextEditedValueIndex;
    HWND hEditBox;
    WNDPROC OldListBoxProc;
    PVARIABLE_DATA VarData;
} DIALOG_DATA, *PDIALOG_DATA;

static DWORD
GatherDataFromEditBox(HWND hwndDlg,
                      PVARIABLE_DATA VarData)
{
    DWORD dwNameLength;
    DWORD dwValueLength;

    dwNameLength = (DWORD)SendDlgItemMessage(hwndDlg, IDC_VARIABLE_NAME, WM_GETTEXTLENGTH, 0, 0);
    dwValueLength = (DWORD)SendDlgItemMessage(hwndDlg, IDC_VARIABLE_VALUE, WM_GETTEXTLENGTH, 0, 0);

    if (dwNameLength == 0 || dwValueLength == 0)
    {
        return 0;
    }
    if (VarData->lpName == NULL)
    {
        VarData->lpName = GlobalAlloc(GPTR, (dwNameLength + 1) * sizeof(TCHAR));
    }
    else if (_tcslen(VarData->lpName) < dwNameLength)
    {
        GlobalFree(VarData->lpName);
        VarData->lpName = GlobalAlloc(GPTR, (dwNameLength + 1) * sizeof(TCHAR));
    }
    SendDlgItemMessage(hwndDlg, IDC_VARIABLE_NAME, WM_GETTEXT, dwNameLength + 1, (LPARAM)VarData->lpName);

    if (VarData->lpRawValue == NULL)
    {
        VarData->lpRawValue = GlobalAlloc(GPTR, (dwValueLength + 1) * sizeof(TCHAR));
    }
    else if (_tcslen(VarData->lpRawValue) < dwValueLength)
    {
        GlobalFree(VarData->lpRawValue);
        VarData->lpRawValue = GlobalAlloc(GPTR, (dwValueLength + 1) * sizeof(TCHAR));
    }
    SendDlgItemMessage(hwndDlg, IDC_VARIABLE_VALUE, WM_GETTEXT, dwValueLength + 1, (LPARAM)VarData->lpRawValue);
    
    return dwValueLength;
}

static DWORD
GatherDataFromListBox(HWND hwndDlg,
                      PVARIABLE_DATA VarData)
{
    DWORD dwValueLength;
    DWORD NumberOfItems;
    DWORD i;
    TCHAR szData[MAX_PATH];

    /* Gather the number of items for the semi-colon */
    NumberOfItems = (DWORD)SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETCOUNT, 0, 0);

    if (NumberOfItems == 0)
    {
        return 0;
    }

    /* Since the last item doesn't need the semi-colon subtract 1 */
    dwValueLength = NumberOfItems - 1;

    for (i = 0; NumberOfItems > i; i++)
    {
        dwValueLength += SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETTEXTLEN, i, 0);
    }

    if (VarData->lpRawValue == NULL)
    {
        VarData->lpRawValue = GlobalAlloc(GPTR, (dwValueLength + 1) * sizeof(TCHAR));
    }
    else if (_tcslen(VarData->lpRawValue) < dwValueLength)
    {
        GlobalFree(VarData->lpRawValue);
        VarData->lpRawValue = GlobalAlloc(GPTR, (dwValueLength + 1) * sizeof(TCHAR));
    }

    /* Copy the first value */
    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETTEXT, 0, (LPARAM)szData);
    StringCchCopy(VarData->lpRawValue, dwValueLength + 1, szData);

    /* After doing so, copy rest of the values while seperating them with a semi-colon except for the last value */
    for (i = 1; NumberOfItems > i; i++)
    {
        StringCchCat(VarData->lpRawValue, dwValueLength + 1, _T(";"));
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETTEXT, i, (LPARAM)szData);
        StringCchCat(VarData->lpRawValue, dwValueLength + 1, szData);
    }
    
    return dwValueLength;
}

static INT
GetSelectedListViewItem(HWND hwndListView)
{
    INT iCount;
    INT iItem;

    iCount = SendMessage(hwndListView,
                         LVM_GETITEMCOUNT,
                         0,
                         0);
    if (iCount != LB_ERR)
    {
        for (iItem = 0; iItem < iCount; iItem++)
        {
            if (SendMessage(hwndListView,
                            LVM_GETITEMSTATE,
                            iItem,
                            (LPARAM) LVIS_SELECTED) == LVIS_SELECTED)
            {
                return iItem;
            }
        }
    }

    return -1;
}

static VOID
AddValuesToListBox(HWND hwndDlg,
                   PDIALOG_DATA DlgData)
{
    DWORD dwValueLength;
    LPTSTR lpTemp;
    LPTSTR lpToken;

    dwValueLength = wcslen(DlgData->VarData->lpRawValue) + 1;
    lpTemp = GlobalAlloc(GPTR, dwValueLength * sizeof(TCHAR));
    
    StringCchCopy(lpTemp, dwValueLength, DlgData->VarData->lpRawValue);
    lpToken = wcstok(lpTemp, _T(";"));

    while (lpToken != NULL)
    {
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_ADDSTRING, 0, (LPARAM)lpToken);
        lpToken = wcstok(NULL, _T(";"));
    }
    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_SETCURSEL, 0, 0);
    DlgData->dwSelectedValueIndex = 0;
    GlobalFree(lpTemp);
}

static VOID
BrowseRequiredFile(HWND hwndDlg)
{
    OPENFILENAME ofn;
    TCHAR szFile[MAX_PATH] = _T("");

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndDlg;
    ofn.lpstrFilter = _T("All Files (*.*)\0*.*\0");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn))
    {
        SetDlgItemText(hwndDlg, IDC_VARIABLE_VALUE, szFile);
    }
}

static VOID
BrowseRequiredFolder(HWND hwndDlg,
                    PDIALOG_DATA DlgData)
{
    BROWSEINFO bi;
    LPITEMIDLIST pidllist;
    TCHAR szDir[MAX_PATH];

    bi.hwndOwner = hwndDlg;
    bi.lpszTitle = _T("");
    bi.pidlRoot = NULL;
    bi.pszDisplayName = _T("");
    bi.ulFlags = BIF_NEWDIALOGSTYLE;

    pidllist = SHBrowseForFolder(&bi);
    if (!pidllist)
    {
        return;
    }

    if (SHGetPathFromIDList(pidllist, szDir))
    {
        if (DlgData->bIsFriendlyUI)
        {
            SendDlgItemMessage(hwndDlg,
                               IDC_LIST_VARIABLE_VALUE,
                               LB_DELETESTRING,
                               DlgData->dwSelectedValueIndex, 0);
            SendDlgItemMessage(hwndDlg,
                               IDC_LIST_VARIABLE_VALUE,
                               LB_INSERTSTRING,
                               DlgData->dwSelectedValueIndex,
                               (LPARAM)szDir);
            SendDlgItemMessage(hwndDlg,
                               IDC_LIST_VARIABLE_VALUE,
                               LB_SETCURSEL,
                               DlgData->dwSelectedValueIndex, 0);
        }
        else
        {
            SetDlgItemText(hwndDlg, IDC_VARIABLE_VALUE, szDir);
        }
    }

    CoTaskMemFree(pidllist);
}

static VOID
ItemEditStarts(HWND hwndDlg,
               PDIALOG_DATA DlgData)
{
    RECT rItem;
    HFONT hFont = NULL;
    TCHAR szStr[MAX_PATH] = _T("");

    /* Get item rect */
    SendDlgItemMessage(hwndDlg,
                       IDC_LIST_VARIABLE_VALUE,
                       LB_GETITEMRECT,
                       DlgData->dwSelectedValueIndex,
                       (LPARAM)&rItem);

    /* Get string from item */
    SendDlgItemMessage(hwndDlg,
                       IDC_LIST_VARIABLE_VALUE,
                       LB_GETTEXT,
                       DlgData->dwSelectedValueIndex,
                       (LPARAM)szStr);

    DlgData->hEditBox = CreateWindow(_T("EDIT"),
                                     szStr,
                                     WS_VISIBLE | WS_CHILD | WS_BORDER |
                                     ES_LEFT | ES_AUTOHSCROLL,
                                     rItem.left, rItem.top,
                                     rItem.right, rItem.bottom - rItem.top,
                                     GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE),
                                     NULL,
                                     hApplet,
                                     NULL);

    hFont = (HFONT)SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, WM_GETFONT, 0, 0);
    SendMessage(DlgData->hEditBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(DlgData->hEditBox, EM_SETSEL, 0, -1);
    SetFocus(DlgData->hEditBox);
    DlgData->dwTextEditedValueIndex = DlgData->dwSelectedValueIndex;
}

static VOID
ItemEditEnds(HWND hwndDlg,
             PDIALOG_DATA DlgData)
{
    TCHAR szStr[MAX_PATH] = _T("");
    TCHAR szStr2[MAX_PATH] = _T("");

    SendMessage(DlgData->hEditBox, WM_GETTEXT, MAX_PATH, (LPARAM)szStr);
    SendDlgItemMessage(hwndDlg,
                       IDC_LIST_VARIABLE_VALUE,
                       LB_GETTEXT,
                       DlgData->dwTextEditedValueIndex,
                       (LPARAM)szStr2);
    SendDlgItemMessage(hwndDlg,
                       IDC_LIST_VARIABLE_VALUE,
                       LB_DELETESTRING,
                       DlgData->dwTextEditedValueIndex, 0);

    if (*szStr || _tcscmp(szStr, szStr2) != 0)
    {
        SendDlgItemMessage(hwndDlg,
                           IDC_LIST_VARIABLE_VALUE,
                           LB_INSERTSTRING,
                           DlgData->dwTextEditedValueIndex, (LPARAM)szStr);
    }
    else
    {
        SendDlgItemMessage(hwndDlg,
                           IDC_LIST_VARIABLE_VALUE,
                           LB_SETCURSEL,
                           DlgData->dwTextEditedValueIndex - 1, 0);

        DlgData->dwSelectedValueIndex = DlgData->dwTextEditedValueIndex - 1;
    }

    DestroyWindow(DlgData->hEditBox);
    DlgData->dwTextEditedValueIndex = -1;
}

static VOID
MoveListItem(HWND hwndDlg,
             PDIALOG_DATA DlgData,
             BOOL bMoveUp)
{
    TCHAR szDest[MAX_PATH];
    DWORD dwSrcIndex, dwDestIndex, dwLastIndex;
    
    dwLastIndex = SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETCOUNT, 0, 0) - 1;
    dwSrcIndex = DlgData->dwSelectedValueIndex;
    dwDestIndex = bMoveUp ? (dwSrcIndex - 1) : (dwSrcIndex + 1);
    
    if ((bMoveUp && dwSrcIndex > 0) || (!bMoveUp && dwSrcIndex < dwLastIndex))
    {
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETTEXT, dwSrcIndex, (LPARAM)szDest);
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_DELETESTRING, dwSrcIndex, 0);
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_INSERTSTRING, dwDestIndex, (LPARAM)szDest);
        DlgData->dwSelectedValueIndex = dwDestIndex;
        SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_SETCURSEL, DlgData->dwSelectedValueIndex, 0);
    }
}

static INT_PTR CALLBACK
ListBoxSubclassProc(HWND hListBox,
                 UINT uMsg,
                 WPARAM wParam,
                 LPARAM lParam)
{
    PDIALOG_DATA DlgData;
    
    DlgData = (PDIALOG_DATA)GetWindowLongPtr(hListBox, GWLP_USERDATA);
    
    switch (uMsg)
    {
        case WM_COMMAND:
        {
            if (HIWORD(wParam) == EN_KILLFOCUS && (HWND)lParam == DlgData->hEditBox)
            {
                ItemEditEnds(GetParent(hListBox), DlgData);
            }
            break;
        }
    }
    
    return CallWindowProc(DlgData->OldListBoxProc, hListBox, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK
EditVariableDlgProc(HWND hwndDlg,
                    UINT uMsg,
                    WPARAM wParam,
                    LPARAM lParam)
{
    PDIALOG_DATA DlgData;

    DlgData = (PDIALOG_DATA)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)lParam);
            DlgData = (PDIALOG_DATA)lParam;

            /* Either get the values from list box or from edit box */
            if (DlgData->bIsFriendlyUI)
            {
                /* Subclass the list box control first */
                DlgData->OldListBoxProc = (WNDPROC)GetWindowLongPtr(GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE), GWLP_WNDPROC);
                SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE), GWLP_USERDATA, (LONG_PTR)DlgData);
                SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE), GWLP_WNDPROC, (LONG_PTR)ListBoxSubclassProc);
                if (DlgData->VarData->lpRawValue != NULL)
                {
                    AddValuesToListBox(hwndDlg, DlgData);
                }
            }
            else
            {
                if (DlgData->VarData->lpName != NULL)
                {
                    SendDlgItemMessage(hwndDlg, IDC_VARIABLE_NAME, WM_SETTEXT, 0, (LPARAM)DlgData->VarData->lpName);
                }

                if (DlgData->VarData->lpRawValue != NULL)
                {
                    SendDlgItemMessage(hwndDlg, IDC_VARIABLE_VALUE, WM_SETTEXT, 0, (LPARAM)DlgData->VarData->lpRawValue);
                }
            }
            break;

        case WM_DESTROY:
        {
            SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE), GWLP_WNDPROC, (LONG_PTR)DlgData->OldListBoxProc);
            break;
        }
        
        case WM_COMMAND:
            switch (HIWORD(wParam))
            {   
                case LBN_DBLCLK:
                {
                    if (!IsWindow(DlgData->hEditBox))
                    {
                        ItemEditStarts(hwndDlg, DlgData);
                    }
                    break;
                }

                case LBN_SELCHANGE:
                {
                    DlgData->dwSelectedValueIndex = (DWORD)SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETCURSEL, 0, 0);
                    break;
                }

                case EN_KILLFOCUS:
                {
                    ItemEditEnds(hwndDlg, DlgData);
                    break;
                }
            }
            
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    LPTSTR p;
                    DWORD dwValueLength;

                    /* Either set the values to the list box or to the edit box */
                    if (DlgData->bIsFriendlyUI)
                    {
                        dwValueLength = GatherDataFromListBox(hwndDlg, DlgData->VarData);
                    }
                    else
                    {
                        dwValueLength = GatherDataFromEditBox(hwndDlg, DlgData->VarData);
                    }

                    if (dwValueLength == 0)
                    {
                        break;
                    }

                    if (DlgData->VarData->lpCookedValue != NULL)
                    {
                        GlobalFree(DlgData->VarData->lpCookedValue);
                        DlgData->VarData->lpCookedValue = NULL;
                    }

                    p = _tcschr(DlgData->VarData->lpRawValue, _T('%'));
                    if (p && _tcschr(++p, _T('%')))
                    {
                        DlgData->VarData->dwType = REG_EXPAND_SZ;
                        DlgData->VarData->lpCookedValue = GlobalAlloc(GPTR, 2 * MAX_PATH * sizeof(TCHAR));

                        ExpandEnvironmentStrings(DlgData->VarData->lpRawValue,
                                                 DlgData->VarData->lpCookedValue,
                                                 2 * MAX_PATH);
                    }
                    else
                    {
                        DlgData->VarData->dwType = REG_SZ;
                        DlgData->VarData->lpCookedValue = GlobalAlloc(GPTR, (dwValueLength + 1) * sizeof(TCHAR));
                        _tcscpy(DlgData->VarData->lpCookedValue, DlgData->VarData->lpRawValue);
                    }

                    EndDialog(hwndDlg, 1);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hwndDlg, 0);
                    return TRUE;

                case IDC_BUTTON_BROWSE_FILE:
                {
                    BrowseRequiredFile(hwndDlg);
                    break;
                }

                case IDC_BUTTON_BROWSE_FOLDER:
                {
                    BrowseRequiredFolder(hwndDlg, DlgData);
                    break;
                }

                case IDC_BUTTON_DELETE:
                {
                    DWORD dwLastIndex;
                    dwLastIndex = SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETCOUNT, 0, 0) - 1;
                    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_DELETESTRING, DlgData->dwSelectedValueIndex, 0);

                    if (dwLastIndex == DlgData->dwSelectedValueIndex)
                    {
                        DlgData->dwSelectedValueIndex--;
                    }

                    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_SETCURSEL, DlgData->dwSelectedValueIndex, 0);
                    break;
                }

                case IDC_BUTTON_MOVE_UP:
                {
                    MoveListItem(hwndDlg, DlgData, TRUE);
                    break;
                }

                case IDC_BUTTON_MOVE_DOWN:
                {
                    MoveListItem(hwndDlg, DlgData, FALSE);
                    break;
                }
                
                case IDC_BUTTON_EDIT_TEXT:
                {
                    TCHAR szStr[128] = _T("");
                    TCHAR szStr2[98] = _T("");

                    LoadString(hApplet, IDS_EVIRONMENT_WARNING, szStr, _countof(szStr));
                    LoadString(hApplet, IDS_EVIRONMENT_WARNING_TITLE, szStr2, _countof(szStr2));
                    
                    if (MessageBox(hwndDlg,
                                   szStr,
                                   szStr2,
                                   MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON1) == IDOK)
                    {
                        EndDialog(hwndDlg, -1);
                    }
                    break;
                }

                case IDC_BUTTON_NEW:
                {
                    DlgData->dwSelectedValueIndex = SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_GETCOUNT, 0, 0);

                    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_INSERTSTRING, DlgData->dwSelectedValueIndex, (LPARAM)_T(""));
                    SendDlgItemMessage(hwndDlg, IDC_LIST_VARIABLE_VALUE, LB_SETCURSEL, DlgData->dwSelectedValueIndex, 0);
                    ItemEditStarts(hwndDlg, DlgData);
                    break;
                }

                case IDC_BUTTON_EDIT:
                {
                    ItemEditStarts(hwndDlg, DlgData);
                    break;
                }
            }
            break;
    }

    return FALSE;
}


static VOID
GetEnvironmentVariables(HWND hwndListView,
                        HKEY hRootKey,
                        LPTSTR lpSubKeyName)
{
    HKEY hKey;
    DWORD dwValues;
    DWORD dwMaxValueNameLength;
    DWORD dwMaxValueDataLength;
    DWORD i;
    LPTSTR lpName;
    LPTSTR lpData;
    LPTSTR lpExpandData;
    DWORD dwNameLength;
    DWORD dwDataLength;
    DWORD dwType;
    PVARIABLE_DATA VarData;

    LV_ITEM lvi;
    int iItem;

    if (RegOpenKeyEx(hRootKey,
                     lpSubKeyName,
                     0,
                     KEY_READ,
                     &hKey))
        return;

    if (RegQueryInfoKey(hKey,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        &dwValues,
                        &dwMaxValueNameLength,
                        &dwMaxValueDataLength,
                        NULL,
                        NULL))
    {
        RegCloseKey(hKey);
        return;
    }

    lpName = GlobalAlloc(GPTR, (dwMaxValueNameLength + 1) * sizeof(TCHAR));
    if (lpName == NULL)
    {
        RegCloseKey(hKey);
        return;
    }

    lpData = GlobalAlloc(GPTR, (dwMaxValueDataLength + 1) * sizeof(TCHAR));
    if (lpData == NULL)
    {
        GlobalFree(lpName);
        RegCloseKey(hKey);
        return;
    }

    lpExpandData = GlobalAlloc(GPTR, 2048 * sizeof(TCHAR));
    if (lpExpandData == NULL)
    {
        GlobalFree(lpName);
        GlobalFree(lpData);
        RegCloseKey(hKey);
        return;
    }

    for (i = 0; i < dwValues; i++)
    {
        dwNameLength = dwMaxValueNameLength + 1;
        dwDataLength = dwMaxValueDataLength + 1;

        if (RegEnumValue(hKey,
                         i,
                         lpName,
                         &dwNameLength,
                         NULL,
                         &dwType,
                         (LPBYTE)lpData,
                         &dwDataLength))
        {
            GlobalFree(lpExpandData);
            GlobalFree(lpName);
            GlobalFree(lpData);
            RegCloseKey(hKey);
            return;
        }

        if (dwType != REG_SZ && dwType != REG_EXPAND_SZ)
            continue;

        VarData = GlobalAlloc(GPTR, sizeof(VARIABLE_DATA));

        VarData->dwType = dwType;

        VarData->lpName = GlobalAlloc(GPTR, (dwNameLength + 1) * sizeof(TCHAR));
        _tcscpy(VarData->lpName, lpName);

        VarData->lpRawValue = GlobalAlloc(GPTR, (dwDataLength + 1) * sizeof(TCHAR));
        _tcscpy(VarData->lpRawValue, lpData);

        ExpandEnvironmentStrings(lpData, lpExpandData, 2048);

        VarData->lpCookedValue = GlobalAlloc(GPTR, (_tcslen(lpExpandData) + 1) * sizeof(TCHAR));
        _tcscpy(VarData->lpCookedValue, lpExpandData);

        memset(&lvi, 0x00, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
        lvi.lParam = (LPARAM)VarData;
        lvi.pszText = VarData->lpName;
        lvi.state = (i == 0) ? LVIS_SELECTED : 0;
        iItem = ListView_InsertItem(hwndListView, &lvi);

        ListView_SetItemText(hwndListView, iItem, 1, VarData->lpCookedValue);
    }

    GlobalFree(lpExpandData);
    GlobalFree(lpName);
    GlobalFree(lpData);
    RegCloseKey(hKey);
}


static VOID
SetEnvironmentDialogListViewColumns(HWND hwndListView)
{
    RECT rect;
    LV_COLUMN column;
    TCHAR szStr[32];

    GetClientRect(hwndListView, &rect);

    memset(&column, 0x00, sizeof(column));
    column.mask=LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
    column.fmt=LVCFMT_LEFT;
    column.cx = (INT)((rect.right - rect.left) * 0.32);
    column.iSubItem = 0;
    LoadString(hApplet, IDS_VARIABLE, szStr, sizeof(szStr) / sizeof(szStr[0]));
    column.pszText = szStr;
    (void)ListView_InsertColumn(hwndListView, 0, &column);

    column.cx = (INT)((rect.right - rect.left) * 0.63);
    column.iSubItem = 1;
    LoadString(hApplet, IDS_VALUE, szStr, sizeof(szStr) / sizeof(szStr[0]));
    column.pszText = szStr;
    (void)ListView_InsertColumn(hwndListView, 1, &column);
}


static VOID
OnInitEnvironmentDialog(HWND hwndDlg)
{
    HWND hwndListView;

    /* Set user environment variables */
    hwndListView = GetDlgItem(hwndDlg, IDC_USER_VARIABLE_LIST);

    (void)ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT);

    SetEnvironmentDialogListViewColumns(hwndListView);

    GetEnvironmentVariables(hwndListView,
                            HKEY_CURRENT_USER,
                            _T("Environment"));

    (void)ListView_SetColumnWidth(hwndListView, 2, LVSCW_AUTOSIZE_USEHEADER);

    ListView_SetItemState(hwndListView, 0,
                          LVIS_FOCUSED | LVIS_SELECTED,
                          LVIS_FOCUSED | LVIS_SELECTED);

    (void)ListView_Update(hwndListView,0);

    /* Set system environment variables */
    hwndListView = GetDlgItem(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);

    (void)ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT);

    SetEnvironmentDialogListViewColumns(hwndListView);

    GetEnvironmentVariables(hwndListView,
                            HKEY_LOCAL_MACHINE,
                            _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"));

    (void)ListView_SetColumnWidth(hwndListView, 2, LVSCW_AUTOSIZE_USEHEADER);

    ListView_SetItemState(hwndListView, 0,
                          LVIS_FOCUSED | LVIS_SELECTED,
                          LVIS_FOCUSED | LVIS_SELECTED);

    (void)ListView_Update(hwndListView, 0);
}


static VOID
OnNewVariable(HWND hwndDlg,
              INT iDlgItem)
{
    HWND hwndListView;
    PDIALOG_DATA DlgData;
    LV_ITEM lvi;
    INT iItem;

    DlgData = GlobalAlloc(GPTR, sizeof(DIALOG_DATA));

    hwndListView = GetDlgItem(hwndDlg, iDlgItem);

    DlgData->bIsFriendlyUI = FALSE;
    DlgData->dwSelectedValueIndex = -1;
    DlgData->dwTextEditedValueIndex = -1;

    DlgData->VarData = GlobalAlloc(GPTR, sizeof(VARIABLE_DATA));

    if (DialogBoxParam(hApplet,
                       MAKEINTRESOURCE(IDD_EDIT_VARIABLE),
                       hwndDlg,
                       EditVariableDlgProc,
                       (LPARAM)DlgData) <= 0)
    {
        if (DlgData->VarData->lpName != NULL)
            GlobalFree(DlgData->VarData->lpName);

        if (DlgData->VarData->lpRawValue != NULL)
            GlobalFree(DlgData->VarData->lpRawValue);

        if (DlgData->VarData->lpCookedValue != NULL)
            GlobalFree(DlgData->VarData->lpCookedValue);

        GlobalFree(DlgData);
    }
    else
    {
        if (DlgData->VarData->lpName != NULL && (DlgData->VarData->lpCookedValue || DlgData->VarData->lpRawValue))
        {
            memset(&lvi, 0x00, sizeof(lvi));
            lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
            lvi.lParam = (LPARAM)DlgData->VarData;
            lvi.pszText = DlgData->VarData->lpName;
            lvi.state = 0;
            iItem = ListView_InsertItem(hwndListView, &lvi);

            ListView_SetItemText(hwndListView, iItem, 1, DlgData->VarData->lpCookedValue);
        }
    }
}


static VOID
OnEditVariable(HWND hwndDlg,
               INT iDlgItem)
{
    HWND hwndListView;
    PDIALOG_DATA DlgData;
    LV_ITEM lvi;
    INT iItem;
    INT DlgID = IDD_EDIT_VARIABLE;
    INT iRet;

    DlgData = GlobalAlloc(GPTR, sizeof(DIALOG_DATA));
    
    DlgData->bIsFriendlyUI = FALSE;
    DlgData->dwSelectedValueIndex = -1;
    DlgData->dwTextEditedValueIndex = -1;
    
    hwndListView = GetDlgItem(hwndDlg, iDlgItem);

    iItem = GetSelectedListViewItem(hwndListView);
    if (iItem != -1)
    {
        memset(&lvi, 0x00, sizeof(lvi));
        lvi.mask = LVIF_PARAM;
        lvi.iItem = iItem;

        if (ListView_GetItem(hwndListView, &lvi))
        {
            DlgData->VarData = (PVARIABLE_DATA)lvi.lParam;

            /* If the value has multiple values and directories then edit value with fancy dialog box */
            if (_tcschr(DlgData->VarData->lpRawValue, ';') != NULL && _tcschr(DlgData->VarData->lpRawValue, '\\') != NULL)
            {
                DlgID = IDD_EDIT_VARIABLE_FANCY;
                DlgData->bIsFriendlyUI = TRUE;
            }

            iRet = DialogBoxParam(hApplet,
                                  MAKEINTRESOURCE(DlgID),
                                  hwndDlg,
                                  EditVariableDlgProc,
                                  (LPARAM)DlgData);

            /* If iRet is less than 0 edit the value and name normally */
            if (iRet < 0)
            {
                DlgID = IDD_EDIT_VARIABLE;
                DlgData->bIsFriendlyUI = FALSE;
                iRet = DialogBoxParam(hApplet,
                                      MAKEINTRESOURCE(DlgID),
                                      hwndDlg,
                                      EditVariableDlgProc,
                                      (LPARAM)DlgData);
            }
            
            if (iRet > 0)
            {
                ListView_SetItemText(hwndListView, iItem, 0, DlgData->VarData->lpName);
                ListView_SetItemText(hwndListView, iItem, 1, DlgData->VarData->lpCookedValue);
            }
        }

        GlobalFree(DlgData);
    }
}


static VOID
OnDeleteVariable(HWND hwndDlg,
                 INT iDlgItem)
{
    HWND hwndListView;
    PVARIABLE_DATA VarData;
    LV_ITEM lvi;
    INT iItem;

    hwndListView = GetDlgItem(hwndDlg, iDlgItem);

    iItem = GetSelectedListViewItem(hwndListView);
    if (iItem != -1)
    {
        memset(&lvi, 0x00, sizeof(lvi));
        lvi.mask = LVIF_PARAM;
        lvi.iItem = iItem;

        if (ListView_GetItem(hwndListView, &lvi))
        {
            VarData = (PVARIABLE_DATA)lvi.lParam;
            if (VarData != NULL)
            {
                if (VarData->lpName != NULL)
                    GlobalFree(VarData->lpName);

                if (VarData->lpRawValue != NULL)
                    GlobalFree(VarData->lpRawValue);

                if (VarData->lpCookedValue != NULL)
                    GlobalFree(VarData->lpCookedValue);

                GlobalFree(VarData);
                lvi.lParam = 0;
            }
        }

        (void)ListView_DeleteItem(hwndListView, iItem);

        /* Select the previous item */
        if (iItem > 0)
            iItem--;

        ListView_SetItemState(hwndListView, iItem,
                              LVIS_FOCUSED | LVIS_SELECTED,
                              LVIS_FOCUSED | LVIS_SELECTED);
    }
}


static VOID
ReleaseListViewItems(HWND hwndDlg,
                     INT iDlgItem)
{
    HWND hwndListView;
    PVARIABLE_DATA VarData;
    LV_ITEM lvi;
    INT nItemCount;
    INT i;

    hwndListView = GetDlgItem(hwndDlg, iDlgItem);

    memset(&lvi, 0x00, sizeof(lvi));

    nItemCount = ListView_GetItemCount(hwndListView);
    for (i = 0; i < nItemCount; i++)
    {
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;

        if (ListView_GetItem(hwndListView, &lvi))
        {
            VarData = (PVARIABLE_DATA)lvi.lParam;
            if (VarData != NULL)
            {
                if (VarData->lpName != NULL)
                    GlobalFree(VarData->lpName);

                if (VarData->lpRawValue != NULL)
                    GlobalFree(VarData->lpRawValue);

                if (VarData->lpCookedValue != NULL)
                    GlobalFree(VarData->lpCookedValue);

                GlobalFree(VarData);
                lvi.lParam = 0;
            }
        }
    }
}


static VOID
SetAllVars(HWND hwndDlg,
           INT iDlgItem)
{
    HWND hwndListView;
    PVARIABLE_DATA VarData;
    LV_ITEM lvi;
    INT iItem;
    HKEY hKey;
    DWORD dwValueCount;
    DWORD dwMaxValueNameLength;
    LPTSTR *aValueArray;
    DWORD dwNameLength;
    DWORD i;
    TCHAR szBuffer[256];
    LPTSTR lpBuffer;

    memset(&lvi, 0x00, sizeof(lvi));

    /* Get the handle to the list box with all system vars in it */
    hwndListView = GetDlgItem(hwndDlg, iDlgItem);
    /* First item is 0 */
    iItem = 0;
    /* Set up struct to retrieve item */
    lvi.mask = LVIF_PARAM;
    lvi.iItem = iItem;

    /* Open or create the key */
    if (RegCreateKeyEx((iDlgItem == IDC_SYSTEM_VARIABLE_LIST ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER),
                       (iDlgItem == IDC_SYSTEM_VARIABLE_LIST ? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment") : _T("Environment")),
                       0,
                       NULL,
                       REG_OPTION_NON_VOLATILE,
                       KEY_WRITE | KEY_READ,
                       NULL,
                       &hKey,
                       NULL))
    {
        return;
    }

    /* Get the number of values and the maximum value name length */
    if (RegQueryInfoKey(hKey,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        &dwValueCount,
                        &dwMaxValueNameLength,
                        NULL,
                        NULL,
                        NULL))
    {
        RegCloseKey(hKey);
        return;
    }

    if (dwValueCount > 0)
    {
        /* Allocate the value array */
        aValueArray = GlobalAlloc(GPTR, dwValueCount * sizeof(LPTSTR));
        if (aValueArray != NULL)
        {
            /* Get all value names */
            for (i = 0; i < dwValueCount; i++)
            {
                dwNameLength = 256;
                if (!RegEnumValue(hKey,
                                  i,
                                  szBuffer,
                                  &dwNameLength,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL))
                {
                    /* Allocate a value name buffer, fill it and attach it to the array */
                    lpBuffer = (LPTSTR)GlobalAlloc(GPTR, (dwNameLength + 1) * sizeof(TCHAR));
                    if (lpBuffer != NULL)
                    {
                        _tcscpy(lpBuffer, szBuffer);
                        aValueArray[i] = lpBuffer;
                    }
                }
            }

            /* Delete all values */
            for (i = 0; i < dwValueCount; i++)
            {
                if (aValueArray[i] != NULL)
                {
                    /* Delete the value */
                    RegDeleteValue(hKey,
                                   aValueArray[i]);

                    /* Free the value name */
                    GlobalFree(aValueArray[i]);
                }
            }

            /* Free the value array */
            GlobalFree(aValueArray);
        }
    }

    /* Loop through all variables */
    while (ListView_GetItem(hwndListView, &lvi))
    {
        /* Get the data in each item */
        VarData = (PVARIABLE_DATA)lvi.lParam;
        if (VarData != NULL)
        {
            /* Set the new value */
            if (RegSetValueEx(hKey,
                              VarData->lpName,
                              0,
                              VarData->dwType,
                              (LPBYTE)VarData->lpRawValue,
                              (DWORD)(_tcslen(VarData->lpRawValue) + 1) * sizeof(TCHAR)))
            {
                RegCloseKey(hKey);
                return;
            }
        }

        /* Fill struct for next item */
        lvi.mask = LVIF_PARAM;
        lvi.iItem = ++iItem;
    }

    RegCloseKey(hKey);
}


static BOOL
OnNotify(HWND hwndDlg, NMHDR *phdr)
{
    switch(phdr->code)
    {
        case NM_DBLCLK:
            if (phdr->idFrom == IDC_USER_VARIABLE_LIST ||
                phdr->idFrom == IDC_SYSTEM_VARIABLE_LIST)
            {
                OnEditVariable(hwndDlg, (INT)phdr->idFrom);
                return TRUE;
            }
            break;

        case LVN_KEYDOWN:
            if (((LPNMLVKEYDOWN)phdr)->wVKey == VK_DELETE &&
                (phdr->idFrom == IDC_USER_VARIABLE_LIST ||
                 phdr->idFrom == IDC_SYSTEM_VARIABLE_LIST))
            {
                OnDeleteVariable(hwndDlg, (INT)phdr->idFrom);
                return TRUE;
            }
            break;
    }

    return FALSE;
}


/* Environment dialog procedure */
INT_PTR CALLBACK
EnvironmentDlgProc(HWND hwndDlg,
                   UINT uMsg,
                   WPARAM wParam,
                   LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            OnInitEnvironmentDialog(hwndDlg);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_USER_VARIABLE_NEW:
                    OnNewVariable(hwndDlg, IDC_USER_VARIABLE_LIST);
                    return TRUE;

                case IDC_USER_VARIABLE_EDIT:
                    OnEditVariable(hwndDlg, IDC_USER_VARIABLE_LIST);
                    return TRUE;

                case IDC_USER_VARIABLE_DELETE:
                    OnDeleteVariable(hwndDlg, IDC_USER_VARIABLE_LIST);
                    return TRUE;

                case IDC_SYSTEM_VARIABLE_NEW:
                    OnNewVariable(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);
                    return TRUE;

                case IDC_SYSTEM_VARIABLE_EDIT:
                    OnEditVariable(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);
                    return TRUE;

                case IDC_SYSTEM_VARIABLE_DELETE:
                    OnDeleteVariable(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);
                    return TRUE;

                case IDOK:
                    SetAllVars(hwndDlg, IDC_USER_VARIABLE_LIST);
                    SetAllVars(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);
                    SendMessage(HWND_BROADCAST, WM_WININICHANGE,
                                0, (LPARAM)_T("Environment"));
                    EndDialog(hwndDlg, 0);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwndDlg, 0);
                    return TRUE;
            }
            break;

        case WM_DESTROY:
            ReleaseListViewItems(hwndDlg, IDC_USER_VARIABLE_LIST);
            ReleaseListViewItems(hwndDlg, IDC_SYSTEM_VARIABLE_LIST);
            break;

        case WM_NOTIFY:
            return OnNotify(hwndDlg, (NMHDR*)lParam);
    }

    return FALSE;
}

/* EOF */
