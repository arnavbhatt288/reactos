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
#include "commctrl.h"
#include <string.h>
#include <strsafe.h>

#define CX_LIST 30160  // Make columns ridiculously long in order to avoid resizing it

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
    DWORD cxMin;
    DWORD cyMin;
    DWORD cxOld;
    DWORD cyOld;
    HWND hEditBox;
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
GatherDataFromListView(HWND hwndListView,
                      PVARIABLE_DATA VarData)
{
    DWORD dwValueLength;
    DWORD NumberOfItems;
    DWORD i;
    TCHAR szData[MAX_PATH];

    /* Gather the number of items for the semi-colon */
    NumberOfItems = (DWORD)ListView_GetItemCount(hwndListView);

    if (NumberOfItems == 0)
    {
        return 0;
    }

    /* Since the last item doesn't need the semi-colon subtract 1 */
    dwValueLength = NumberOfItems - 1;

    for (i = 0; NumberOfItems > i; i++)
    {
        ListView_GetItemText(hwndListView,
                             i,
                             0,
                             szData,
                             MAX_PATH);
        dwValueLength += _tcslen(szData);
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


    /* Copy the variable values while seperating them with a semi-colon except for the last value */
    for (i = 0; NumberOfItems > i; i++)
    {
        if (i > 0)
        {
            StringCchCat(VarData->lpRawValue, dwValueLength + 1, _T(";"));
        }
        ListView_GetItemText(hwndListView,
                             i,
                             0,
                             szData,
                             MAX_PATH);
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

static LRESULT CALLBACK
ListViewSubclassProc(HWND hListBox,
                    UINT uMsg,
                    WPARAM wParam,
                    LPARAM lParam,
                    UINT_PTR uIdSubclass,
                    DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_DESTROY:
        {
            RemoveWindowSubclass(hListBox, ListViewSubclassProc, uIdSubclass);
            break;
        }

        /* Whenever the control is resized make sure it doesn't spawn the horizontal scrollbar */
        case WM_SIZE:
        {
            ShowScrollBar(hListBox, SB_HORZ, FALSE);
            break;
        }
    }

    return DefSubclassProc(hListBox, uMsg, wParam, lParam);
}

static VOID
AddValuesToList(HWND hwndDlg,
                   PDIALOG_DATA DlgData)
{
    LV_COLUMN column;
    LV_ITEM lvi;
    RECT rItem;

    DWORD dwValueLength;
    DWORD i;
    HWND hwndListView;
    LPTSTR lpTemp;
    LPTSTR lpToken;

    memset(&column, 0x00, sizeof(column));
    memset(&lvi, 0x00, sizeof(lvi));

    hwndListView = GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE);

    GetClientRect(hwndListView, &rItem);
    
    column.mask = LVCF_WIDTH;
    column.cx = CX_LIST;
    ListView_InsertColumn(hwndListView, 0, &column);
    ShowScrollBar(hwndListView, SB_HORZ, FALSE);

    lvi.mask = LVIF_TEXT | LVIF_STATE;
    lvi.cchTextMax = MAX_PATH;
    lvi.iSubItem = 0;
    
    dwValueLength = wcslen(DlgData->VarData->lpRawValue) + 1;
    lpTemp = GlobalAlloc(GPTR, dwValueLength * sizeof(TCHAR));
    
    StringCchCopy(lpTemp, dwValueLength, DlgData->VarData->lpRawValue);
    lpToken = wcstok(lpTemp, _T(";"));

    i = 0;

    while (lpToken != NULL)
    {
        lvi.iItem = i;
        lvi.pszText = lpToken;
        lvi.state = (i == 0) ? LVIS_SELECTED : 0;
        ListView_InsertItem(hwndListView, &lvi);
        lpToken = wcstok(NULL, _T(";"));
        i++;
    }

    DlgData->dwSelectedValueIndex = 0;
    ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
    ListView_SetItemState(hwndListView, DlgData->dwSelectedValueIndex,
                          LVIS_FOCUSED | LVIS_SELECTED,
                          LVIS_FOCUSED | LVIS_SELECTED);

    ListView_Update(hwndListView, DlgData->dwSelectedValueIndex);
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
MoveListItem(HWND hwndDlg,
             PDIALOG_DATA DlgData,
             BOOL bMoveUp)
{
    TCHAR szDest[MAX_PATH];
    TCHAR szSource[MAX_PATH];
    HWND hwndListView;
    DWORD dwSrcIndex, dwDestIndex, dwLastIndex;
    
    hwndListView = GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE);

    dwLastIndex = (DWORD)ListView_GetItemCount(hwndListView) - 1;
    dwSrcIndex = DlgData->dwSelectedValueIndex;
    dwDestIndex = bMoveUp ? (dwSrcIndex - 1) : (dwSrcIndex + 1);
    
    if ((bMoveUp && dwSrcIndex > 0) || (!bMoveUp && dwSrcIndex < dwLastIndex))
    {
        ListView_GetItemText(hwndListView,
                             dwSrcIndex,
                             0,
                             szDest,
                             MAX_PATH);
        ListView_GetItemText(hwndListView,
                             dwDestIndex,
                             0,
                             szSource,
                             MAX_PATH);

        ListView_SetItemText(hwndListView,
                             dwDestIndex,
                             0,
                             szDest);
        ListView_SetItemText(hwndListView,
                             dwSrcIndex,
                             0,
                             szSource);

        DlgData->dwSelectedValueIndex = dwDestIndex;
        ListView_SetItemState(hwndListView, DlgData->dwSelectedValueIndex,
                              LVIS_FOCUSED | LVIS_SELECTED,
                              LVIS_FOCUSED | LVIS_SELECTED);
    }
}

static VOID
OnSize(HWND hwndDlg, PDIALOG_DATA DlgData, DWORD cx, DWORD cy)
{
    RECT rect;
    HDWP hdwp;
    HWND hItemWnd;
    DWORD y;

    y = 0;

    if (cx < DlgData->cxMin)
    {
        cx = DlgData->cxMin;
    }
    if (cy < DlgData->cyMin)
    {
        cy = DlgData->cyMin;
    }

    if ((cx != DlgData->cxOld) || (cy != DlgData->cyOld))
    {
        hdwp = BeginDeferWindowPos(10);

        GetWindowRect(hwndDlg, &rect);

        /* For list view control */
        hItemWnd = GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (cy >= DlgData->cyOld)
        {
            y += (cy - DlgData->cyOld) / 2 + (cy - DlgData->cyOld) % 2;
        }
        else
        {
            y -= (DlgData->cyOld - cy) / 2 + (DlgData->cyOld - cy) % 2;
        }

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left, rect.top,
                                  (rect.right - rect.left) + (cx - DlgData->cxOld),
                                  (rect.bottom - rect.top) + y,
                                   SWP_NOZORDER | SWP_NOACTIVATE);
        }

        /* For buttons */
        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_BROWSE_FOLDER);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_NEW);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_EDIT);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_DELETE);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_MOVE_UP);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_MOVE_DOWN);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDC_BUTTON_EDIT_TEXT);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top,
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDOK);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top + (cy - DlgData->cyOld),
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        hItemWnd = GetDlgItem(hwndDlg, IDCANCEL);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top + (cy - DlgData->cyOld),
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        /* For the size grip */
        hItemWnd = GetDlgItem(hwndDlg, IDC_DIALOG_GRIP);
        GetWindowRect(hItemWnd, &rect);
        MapWindowPoints(HWND_DESKTOP, hwndDlg, (LPPOINT)&rect, sizeof(RECT)/sizeof(POINT));

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp,
                                  hItemWnd,
                                  0,
                                  rect.left + (cx - DlgData->cxOld),
                                  rect.top + (cy - DlgData->cyOld),
                                  0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        if (hdwp)
        {
            EndDeferWindowPos(hdwp);
        }

        DlgData->cxOld = cx;
        DlgData->cyOld = cy;
    }
}

static BOOL
OnBeginLabelEdit(NMLVDISPINFO* pnmv)
{
    HWND hwndEdit;

    hwndEdit = ListView_GetEditControl(pnmv->hdr.hwndFrom);
    if (hwndEdit == NULL)
    {
        return TRUE;
    }

    SendMessage(hwndEdit, EM_SETLIMITTEXT, MAX_PATH, 0);

    return FALSE;
}

static BOOL
OnEndLabelEdit(NMLVDISPINFO* pnmv)
{
    TCHAR szOldDir[MAX_PATH];
    TCHAR szNewDir[MAX_PATH];

    /* Leave, if there is no valid listview item */
    if (pnmv->item.iItem == -1)
    {
        return FALSE;
    }

    /* Get the new user name */
    ListView_GetItemText(pnmv->hdr.hwndFrom,
                         pnmv->item.iItem, 0,
                         szOldDir,
                         MAX_PATH);

    /* Delete the item, if there is no text */
    if (pnmv->item.pszText == NULL)
    {
        ListView_DeleteItem(pnmv->hdr.hwndFrom, pnmv->item.iItem);
        ListView_SetItemState(pnmv->hdr.hwndFrom, pnmv->item.iItem - 1,
                              LVIS_FOCUSED | LVIS_SELECTED,
                              LVIS_FOCUSED | LVIS_SELECTED);
        return FALSE;
    }

    /* Get the new user name */
    _tcscpy(szNewDir, pnmv->item.pszText);

    /* Leave, if the user name was not changed */
    if (_tcscmp(szOldDir, szNewDir) == 0)
    {
        return FALSE;
    }

    /* Update the listview item */
    ListView_SetItemText(pnmv->hdr.hwndFrom,
                         pnmv->item.iItem, 0,
                         szNewDir);

    return TRUE;
}

static BOOL
OnNotifyEditVariableDlg(HWND hwndDlg, PDIALOG_DATA DlgData, NMHDR *phdr)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)phdr;

    switch (phdr->idFrom)
    {
        case IDC_LIST_VARIABLE_VALUE:
            switch(phdr->code)
            {
                case NM_CLICK:
                {
                    DlgData->dwSelectedValueIndex = lpnmlv->iItem;
                    break;
                }

                case NM_DBLCLK:
                {
                    if (lpnmlv->iItem == -1)
                    {
                        SendMessage(GetDlgItem(hwndDlg, IDC_BUTTON_NEW), BM_CLICK, 0, 0);
                    }
                    else
                    {
                        ListView_EditLabel(GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE), DlgData->dwSelectedValueIndex);
                    }
                    break;
                }

                case LVN_BEGINLABELEDIT:
                {
                    return OnBeginLabelEdit((NMLVDISPINFO*)phdr);
                }

                case LVN_ENDLABELEDIT:
                {
                    return OnEndLabelEdit((NMLVDISPINFO*)phdr);
                }
            }
            break;
    }

    return FALSE;
}

static INT_PTR CALLBACK
EditVariableDlgProc(HWND hwndDlg,
                    UINT uMsg,
                    WPARAM wParam,
                    LPARAM lParam)
{
    PDIALOG_DATA DlgData;
    HWND hwndListView;

    DlgData = (PDIALOG_DATA)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    hwndListView = GetDlgItem(hwndDlg, IDC_LIST_VARIABLE_VALUE);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            RECT rect;

            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)lParam);
            DlgData = (PDIALOG_DATA)lParam;

            /* Either get the values from list box or from edit box */
            if (DlgData->bIsFriendlyUI)
            {
                /* Subclass the listview control first */
                SetWindowSubclass(hwndListView, ListViewSubclassProc, 1, 0);

                if (DlgData->VarData->lpRawValue != NULL)
                {
                    AddValuesToList(hwndDlg, DlgData);
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

            GetClientRect(hwndDlg, &rect);
            DlgData->cxMin = DlgData->cxOld = rect.right - rect.left;
            DlgData->cyMin = DlgData->cyOld = rect.bottom - rect.top;
            break;
        }
        
        case WM_SIZE:
        {
            OnSize(hwndDlg, DlgData, LOWORD(lParam), HIWORD(lParam));
            break;
        }
        
        case WM_NOTIFY:
        {
            return OnNotifyEditVariableDlg(hwndDlg, DlgData, (NMHDR*)lParam);
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    LPTSTR p;
                    DWORD dwValueLength;

                    /* Either set the values to the list box or to the edit box */
                    if (DlgData->bIsFriendlyUI)
                    {
                        dwValueLength = GatherDataFromListView(hwndListView, DlgData->VarData);
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
                    dwLastIndex = (DWORD)ListView_GetItemCount(hwndListView) - 1;
                    ListView_DeleteItem(hwndListView, DlgData->dwSelectedValueIndex);

                    if (dwLastIndex == DlgData->dwSelectedValueIndex)
                    {
                        DlgData->dwSelectedValueIndex--;
                    }

                    ListView_SetItemState(hwndListView, DlgData->dwSelectedValueIndex,
                                          LVIS_FOCUSED | LVIS_SELECTED,
                                          LVIS_FOCUSED | LVIS_SELECTED);
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
                    LV_ITEM lvi;
                    
                    DlgData->dwSelectedValueIndex = (DWORD)ListView_GetItemCount(hwndListView);

                    memset(&lvi, 0x00, sizeof(lvi));
                    lvi.mask = LVIF_TEXT | LVIF_STATE;
                    lvi.cchTextMax = MAX_PATH;
                    lvi.pszText = _T("");
                    lvi.iItem = DlgData->dwSelectedValueIndex;
                    lvi.iSubItem = 0;
                    ListView_InsertItem(hwndListView, &lvi);

                    ListView_EditLabel(hwndListView, DlgData->dwSelectedValueIndex);
                    break;
                }

                case IDC_BUTTON_EDIT:
                {
                    ListView_EditLabel(hwndListView, DlgData->dwSelectedValueIndex);
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
