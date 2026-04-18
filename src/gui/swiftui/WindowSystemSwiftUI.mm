#include "Common/precompiled.h"

#include "interface/WindowSystem.h"

#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "config/ActiveSettings.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

extern "C" void *CemuCreateSwiftUIRootViewController(void);

@interface CemuAppDelegate : NSObject <NSApplicationDelegate>
- (void)setupMenuBar;
- (void)quitApp:(id)sender;
- (void)openGame:(id)sender;
- (void)openPreferences:(id)sender;
- (void)toggleFullscreen:(id)sender;
- (void)showHelp:(id)sender;
- (void)showAbout:(id)sender;
@end

namespace {
extern WindowSystem::WindowInfo g_window_info;
extern NSWindow *g_main_window;
extern CemuAppDelegate *g_app_delegate;
bool PrepareLaunchPath(const fs::path &launchPath, std::string &errorOut);
}

@implementation CemuAppDelegate

- (void)setupMenuBar {
  NSMenu *mainMenu = [[NSMenu alloc] init];
  
  // File menu
  NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
  NSMenuItem *fileMenuItem = [mainMenu addItemWithTitle:@"File" action:nil keyEquivalent:@""];
  [mainMenu setSubmenu:fileMenu forItem:fileMenuItem];
  
  NSMenuItem *openGameItem = [fileMenu addItemWithTitle:@"Open Game..." action:@selector(openGame:) keyEquivalent:@"o"];
  [openGameItem setTarget:self];
  [fileMenu addItem:[NSMenuItem separatorItem]];
  NSMenuItem *quitItem = [fileMenu addItemWithTitle:@"Quit Cemu" action:@selector(quitApp:) keyEquivalent:@"q"];
  [quitItem setTarget:self];
  
  // Edit menu
  NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  NSMenuItem *editMenuItem = [mainMenu addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
  [mainMenu setSubmenu:editMenu forItem:editMenuItem];
  
  NSMenuItem *preferencesItem = [editMenu addItemWithTitle:@"Preferences..." action:@selector(openPreferences:) keyEquivalent:@","];
  [preferencesItem setTarget:self];
  
  // View menu
  NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
  NSMenuItem *viewMenuItem = [mainMenu addItemWithTitle:@"View" action:nil keyEquivalent:@""];
  [mainMenu setSubmenu:viewMenu forItem:viewMenuItem];
  
  NSMenuItem *fullscreenItem = [viewMenu addItemWithTitle:@"Toggle Fullscreen" action:@selector(toggleFullscreen:) keyEquivalent:@"f"];
  [fullscreenItem setTarget:self];
  
  // Help menu
  NSMenu *helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
  NSMenuItem *helpMenuItem = [mainMenu addItemWithTitle:@"Help" action:nil keyEquivalent:@""];
  [mainMenu setSubmenu:helpMenu forItem:helpMenuItem];
  
  NSMenuItem *helpItem = [helpMenu addItemWithTitle:@"Cemu Help" action:@selector(showHelp:) keyEquivalent:@""];
  [helpItem setTarget:self];
  NSMenuItem *aboutItem = [helpMenu addItemWithTitle:@"About Cemu" action:@selector(showAbout:) keyEquivalent:@""];
  [aboutItem setTarget:self];
  
  [NSApp setMainMenu:mainMenu];
}

- (void)quitApp:(id)sender {
  [NSApp terminate:sender];
}

- (void)openGame:(id)sender {
  if (CafeSystem::IsTitleRunning()) {
    WindowSystem::ShowErrorDialog("A title is already running.",
                                  "Launch blocked");
    return;
  }

  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setCanChooseFiles:YES];
  [panel setCanChooseDirectories:NO];
  [panel setAllowsMultipleSelection:NO];
  [panel setAllowedContentTypes:@[
    [UTType typeWithFilenameExtension:@"wud"],
    [UTType typeWithFilenameExtension:@"wux"],
    [UTType typeWithFilenameExtension:@"wua"],
    [UTType typeWithFilenameExtension:@"wuhb"],
    [UTType typeWithFilenameExtension:@"iso"],
    [UTType typeWithFilenameExtension:@"rpx"],
    [UTType typeWithFilenameExtension:@"elf"],
    [UTType typeWithFilenameExtension:@"tmd"]
  ]];

  if ([panel runModal] != NSModalResponseOK || panel.URL == nil)
    return;

  NSString *pathString = panel.URL.path;
  if (pathString.length == 0)
    return;

  fs::path launchPath = _utf8ToPath(std::string(pathString.UTF8String));
  std::string errorMessage;
  if (!PrepareLaunchPath(launchPath, errorMessage)) {
    WindowSystem::ShowErrorDialog(errorMessage, "Failed to launch game");
    return;
  }

  WindowSystem::UpdateWindowTitles(false, true, 0.0);
  CafeSystem::LaunchForegroundTitle();
  WindowSystem::NotifyGameLoaded();

  const std::string titleName = CafeSystem::GetForegroundTitleName();
  if (!titleName.empty() && g_main_window) {
    [g_main_window setTitle:[NSString stringWithUTF8String:titleName.c_str()]];
  }
}

- (void)openPreferences:(id)sender {
  fs::path configPath = ActiveSettings::GetConfigPath();
  std::string configPathUtf8 = _pathToUtf8(configPath);
  NSString *configNSString =
      [NSString stringWithUTF8String:configPathUtf8.c_str()];
  if (configNSString.length == 0)
    return;

  NSURL *configURL = [NSURL fileURLWithPath:configNSString isDirectory:YES];
  [[NSWorkspace sharedWorkspace] openURL:configURL];
}

- (void)toggleFullscreen:(id)sender {
  if (!g_main_window)
    return;

  [g_main_window toggleFullScreen:nil];
  g_window_info.is_fullscreen = !g_window_info.is_fullscreen.load();
}

- (void)showHelp:(id)sender {
  NSURL *helpURL = [NSURL URLWithString:@"https://wiki.cemu.info"];
  if (helpURL)
    [[NSWorkspace sharedWorkspace] openURL:helpURL];
}

- (void)showAbout:(id)sender {
  NSAlert *about = [[NSAlert alloc] init];
  [about setAlertStyle:NSAlertStyleInformational];
  [about setMessageText:@"About Cemu"];
  [about setInformativeText:@"Cemu - Wii U Emulator\nSwiftUI macOS GUI"];
  [about addButtonWithTitle:@"OK"];
  [about runModal];
}

@end

namespace {
WindowSystem::WindowInfo g_window_info{};
NSWindow *g_main_window = nil;
CemuAppDelegate *g_app_delegate = nil;

bool PrepareLaunchPath(const fs::path &launchPath, std::string &errorOut) {
  TitleInfo launchTitle{launchPath};
  if (launchTitle.IsValid()) {
    CafeTitleList::AddTitleFromPath(launchPath);

    TitleId baseTitleId;
    if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(),
                                        baseTitleId)) {
      errorOut =
          "Unable to launch game because the base files were not found.";
      return false;
    }

    CafeSystem::PREPARE_STATUS_CODE status =
        CafeSystem::PrepareForegroundTitle(baseTitleId);
    if (status == CafeSystem::PREPARE_STATUS_CODE::UNABLE_TO_MOUNT) {
      errorOut =
          "Unable to mount title. Make sure your game paths are valid and "
          "refresh the game list.";
      return false;
    }
    if (status != CafeSystem::PREPARE_STATUS_CODE::SUCCESS) {
      errorOut = "Failed to prepare the selected game for launch.";
      return false;
    }
    return true;
  }

  CafeTitleFileType fileType = DetermineCafeSystemFileType(launchPath);
  if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF) {
    CafeSystem::PREPARE_STATUS_CODE status =
        CafeSystem::PrepareForegroundTitleFromStandaloneRPX(launchPath);
    if (status != CafeSystem::PREPARE_STATUS_CODE::SUCCESS) {
      errorOut = "Failed to prepare standalone RPX/ELF executable.";
      return false;
    }
    return true;
  }

  errorOut = "Unsupported or invalid Wii U title path.";
  return false;
}
}  // namespace

void WindowSystem::ShowErrorDialog(
    std::string_view message, std::string_view title,
    std::optional<WindowSystem::ErrorCategory> /*errorCategory*/) {
  @autoreleasepool {
    NSAlert *alert = [[NSAlert alloc] init];
    std::string titleCopy(title);
    NSString *alertTitle =
        titleCopy.empty() ? @"Error"
                          : [NSString stringWithUTF8String:titleCopy.c_str()];
    NSString *alertMessage =
        [NSString stringWithUTF8String:std::string(message).c_str()];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert setMessageText:alertTitle];
    [alert setInformativeText:alertMessage ?: @""];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
  }
}

void WindowSystem::Create() {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    
    // Setup application delegate with menu bar
    g_app_delegate = [[CemuAppDelegate alloc] init];
    [app setDelegate:g_app_delegate];
    [g_app_delegate setupMenuBar];

    const NSRect frame = NSMakeRect(120.0, 120.0, 1280.0, 720.0);
    const NSWindowStyleMask style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable |
        NSWindowStyleMaskUnifiedTitleAndToolbar;

    g_main_window = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [g_main_window setTitle:@"Cemu"];
    [g_main_window setTitlebarAppearsTransparent:NO];
    
    // Instantiate SwiftUI-backed root controller via explicit Swift C symbol.
    NSViewController *rootViewController = nil;
    if (void *swiftControllerPtr = CemuCreateSwiftUIRootViewController()) {
      id swiftControllerObj = (__bridge id)swiftControllerPtr;
      if ([swiftControllerObj isKindOfClass:[NSViewController class]]) {
        rootViewController = (NSViewController *)swiftControllerObj;
      }
    }
    if (!rootViewController) {
      rootViewController = [[NSViewController alloc] init];
      NSView *contentView = [[NSView alloc] initWithFrame:frame];
      [contentView setWantsLayer:YES];
      contentView.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;

      NSTextField *fallbackLabel = [NSTextField labelWithString:
          @"SwiftUI root view not found.\nUsing AppKit fallback view."];
      [fallbackLabel setFont:[NSFont systemFontOfSize:16 weight:NSFontWeightMedium]];
      [fallbackLabel setTextColor:NSColor.secondaryLabelColor];
      [fallbackLabel setAlignment:NSTextAlignmentCenter];
      [fallbackLabel setFrame:NSMakeRect(40, frame.size.height / 2 - 20,
                                         frame.size.width - 80, 60)];
      [fallbackLabel setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin | NSViewMaxYMargin];
      [contentView addSubview:fallbackLabel];

      rootViewController.view = contentView;
    }
    
    [g_main_window setContentViewController:rootViewController];
    
    [g_main_window makeKeyAndOrderFront:nil];
    [app activateIgnoringOtherApps:YES];

    g_window_info.app_active = true;
    g_window_info.width = static_cast<int32_t>(frame.size.width);
    g_window_info.height = static_cast<int32_t>(frame.size.height);
    g_window_info.phys_width = g_window_info.width.load();
    g_window_info.phys_height = g_window_info.height.load();
    g_window_info.dpi_scale = 1.0;
    g_window_info.pad_open = false;
    g_window_info.pad_width = 0;
    g_window_info.pad_height = 0;
    g_window_info.phys_pad_width = 0;
    g_window_info.phys_pad_height = 0;
    g_window_info.pad_dpi_scale = 1.0;
    g_window_info.is_fullscreen = false;
    g_window_info.debugger_focused = false;
    g_window_info.window_main.backend =
        WindowSystem::WindowHandleInfo::Backend::Cocoa;
    g_window_info.window_main.display = nullptr;
    g_window_info.window_main.surface = (__bridge void *)g_main_window;

    [NSApp run];
  }
}

WindowSystem::WindowInfo &WindowSystem::GetWindowInfo() {
  return g_window_info;
}

void WindowSystem::UpdateWindowTitles(bool isIdle, bool isLoading, double fps) {
  if (!g_main_window)
    return;

  NSString *title = nil;
  if (isIdle)
    title = @"Cemu";
  else if (isLoading)
    title = @"Cemu - Loading...";
  else
    title = [NSString stringWithFormat:@"Cemu - FPS: %.2f", fps];

  [g_main_window setTitle:title];
}

void WindowSystem::GetWindowSize(int &w, int &h) {
  w = g_window_info.width;
  h = g_window_info.height;
}

void WindowSystem::GetPadWindowSize(int &w, int &h) {
  w = 0;
  h = 0;
}

void WindowSystem::GetWindowPhysSize(int &w, int &h) {
  w = g_window_info.phys_width;
  h = g_window_info.phys_height;
}

void WindowSystem::GetPadWindowPhysSize(int &w, int &h) {
  w = 0;
  h = 0;
}

double WindowSystem::GetWindowDPIScale() { return g_window_info.dpi_scale; }

double WindowSystem::GetPadDPIScale() { return 1.0; }

bool WindowSystem::IsPadWindowOpen() { return false; }

bool WindowSystem::IsKeyDown(uint32 key) {
  return g_window_info.get_keystate(key);
}

bool WindowSystem::IsKeyDown(PlatformKeyCodes key) {
  switch (key) {
  case PlatformKeyCodes::LCONTROL:
    return IsKeyDown(0x3B);
  case PlatformKeyCodes::RCONTROL:
    return IsKeyDown(0x3E);
  case PlatformKeyCodes::TAB:
    return IsKeyDown(0x30);
  case PlatformKeyCodes::ESCAPE:
    return IsKeyDown(0x35);
  default:
    return false;
  }
}

std::string WindowSystem::GetKeyCodeName(uint32 key) {
  return fmt::format("key_{}", key);
}

bool WindowSystem::InputConfigWindowHasFocus() { return false; }

void WindowSystem::NotifyGameLoaded() {}

void WindowSystem::NotifyGameExited() {}

void WindowSystem::RefreshGameList() { CafeTitleList::Refresh(); }

void WindowSystem::CaptureInput(const ControllerState & /*currentState*/,
                                const ControllerState & /*lastState*/) {}

bool WindowSystem::IsFullScreen() { return g_window_info.is_fullscreen; }
