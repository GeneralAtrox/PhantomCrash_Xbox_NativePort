#include "desktop_save_location_dialog.hpp"
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <system_error>
#include <vector>

namespace phantomcrash {
namespace {
enum : int { id_caption = 101, id_path, id_open };

std::vector<WORD> dialog_template() {
  std::vector<WORD> words(sizeof(DLGTEMPLATE) / sizeof(WORD));
  auto &dialog = *reinterpret_cast<DLGTEMPLATE *>(words.data());
  dialog.style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
  dialog.cx = 300;
  dialog.cy = 66;
  auto append = [&](const wchar_t *text) {
    for (; *text; ++text) {
      words.push_back(static_cast<WORD>(*text));
    }
    words.push_back(0);
  };
  words.push_back(0); // no menu
  words.push_back(0); // default dialog class
  append(L"Save Data Location");
  words.push_back(9); // point size
  append(L"Segoe UI");
  return words;
}

HWND add(HWND dialog, const wchar_t *type, const wchar_t *text, DWORD style, int id, int x, int y,
         int cx, int cy) {
  RECT r{x, y, x + cx, y + cy};
  MapDialogRect(dialog, &r);
  HWND child =
      CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left,
                      r.bottom - r.top, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(child, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), TRUE);
  return child;
}

void initialize(HWND dialog, const std::wstring &folder) {
  add(dialog, L"STATIC",
      L"Settings, the game in progress and the save slots are kept in this folder:", SS_LEFT,
      id_caption, 10, 8, 280, 10);
  add(dialog, L"EDIT", folder.c_str(), ES_READONLY | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP,
      id_path, 10, 22, 226, 13);
  add(dialog, L"BUTTON", L"&Open", BS_PUSHBUTTON | WS_TABSTOP, id_open, 240, 21, 50, 14);
  add(dialog, L"BUTTON", L"Close", BS_DEFPUSHBUTTON | WS_TABSTOP, IDCANCEL, 240, 45, 50, 14);
  SetFocus(GetDlgItem(dialog, id_open));
}

void open_folder(HWND dialog, const std::wstring &folder) {
  const auto result = reinterpret_cast<INT_PTR>(
      ShellExecuteW(dialog, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32) {
    MessageBoxW(dialog, (L"Unable to open " + folder).c_str(), L"Save Data Location",
                MB_OK | MB_ICONWARNING);
  }
}

INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
  const auto *folder = reinterpret_cast<const std::wstring *>(GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
  case WM_INITDIALOG:
    SetWindowLongPtrW(dialog, DWLP_USER, lparam);
    initialize(dialog, *reinterpret_cast<const std::wstring *>(lparam));
    return FALSE;
  case WM_COMMAND:
    if (!folder) {
      return FALSE;
    }
    switch (LOWORD(wparam)) {
    case id_open:
      open_folder(dialog, *folder);
      return TRUE;
    case IDOK:
    case IDCANCEL:
      EndDialog(dialog, IDCANCEL);
      return TRUE;
    }
    return FALSE;
  }
  return FALSE;
}
} // namespace

void show_save_location(void *owner, const std::filesystem::path &folder) {
  std::error_code error;
  std::filesystem::path shown = std::filesystem::absolute(folder, error);
  if (error) {
    shown = folder;
  }
  const std::wstring text = shown.make_preferred().wstring();
  const auto words = dialog_template();
  DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                          reinterpret_cast<const DLGTEMPLATE *>(words.data()),
                          static_cast<HWND>(owner), &procedure, reinterpret_cast<LPARAM>(&text));
}
} // namespace phantomcrash
#else
namespace phantomcrash {
void show_save_location(void *, const std::filesystem::path &) {}
} // namespace phantomcrash
#endif
