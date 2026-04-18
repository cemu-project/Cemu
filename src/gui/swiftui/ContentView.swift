import AppKit
import SwiftUI

@objc(CemuSwiftUIRootViewController)
final class CemuSwiftUIRootViewController: NSViewController {
    override func loadView() {
        self.view = NSHostingView(rootView: ContentView())
    }
}

@_cdecl("CemuCreateSwiftUIRootViewController")
public func CemuCreateSwiftUIRootViewController() -> UnsafeMutableRawPointer {
    let controller = CemuSwiftUIRootViewController()
    return Unmanaged.passRetained(controller).autorelease().toOpaque()
}

struct ContentView: View {
    @State private var selectedTitleID: UInt64?
    @State private var showUpdatingBanner = false
    @State private var games: [GameItem] = [
        GameItem(
            titleID: 0x0005_0000_101C_9400,
            name: "The Legend of Zelda: Breath of the Wild",
            version: "208",
            dlc: "80",
            played: "37 hours 42 minutes",
            lastPlayed: "4/12/26",
            region: "EUR"
        ),
        GameItem(
            titleID: 0x0005_0000_1010_EC00,
            name: "Mario Kart 8",
            version: "64",
            dlc: "48",
            played: "12 hours 5 minutes",
            lastPlayed: "3/28/26",
            region: "USA"
        ),
        GameItem(
            titleID: 0x0005_0000_1017_6A00,
            name: "Splatoon",
            version: "80",
            dlc: "",
            played: "1 hour 12 minutes",
            lastPlayed: "never",
            region: "JPN"
        ),
        GameItem(
            titleID: 0x0005_0000_1014_4F00,
            name: "Super Smash Bros. for Wii U",
            version: "288",
            dlc: "192",
            played: "",
            lastPlayed: "never",
            region: "USA"
        ),
    ]

    var body: some View {
        VStack(spacing: 0) {
            GameListHeaderView()

            Divider()

            ScrollView {
                LazyVStack(spacing: 0) {
                    ForEach(Array(games.enumerated()), id: \.element.id) { index, game in
                        GameListRowView(
                            game: game,
                            isSelected: selectedTitleID == game.titleID,
                            isAlternateRow: index.isMultiple(of: 2)
                        )
                        .contentShape(Rectangle())
                        .onTapGesture {
                            selectedTitleID = game.titleID
                        }

                        Divider()
                    }
                }
            }
            .background(Color(nsColor: .controlBackgroundColor))

            if showUpdatingBanner {
                GameListInfoBarView(message: "Updating game list...") {
                    withAnimation(.easeInOut(duration: 0.2)) {
                        showUpdatingBanner = false
                    }
                }
                .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .toolbar {
            ToolbarItemGroup(placement: .automatic) {
                Button(action: refreshGameList) {
                    Image(systemName: "arrow.clockwise")
                }
                .help("Refresh game list")

                Button(action: openSettings) {
                    Image(systemName: "gear")
                }
                .help("Settings")
            }
        }
        .frame(minWidth: 900, minHeight: 480)
    }

    private func refreshGameList() {
        withAnimation(.easeInOut(duration: 0.2)) {
            showUpdatingBanner = true
        }
    }

    private func openSettings() {
        // SwiftUI migration placeholder: the menu action exists in WindowSystemSwiftUI.
    }
}

struct GameListHeaderView: View {
    var body: some View {
        HStack(spacing: 0) {
            headerCell("Icon", width: 66, alignment: .center)
            headerCell("Game", width: 340, alignment: .leading)
            headerCell("Version", width: 84, alignment: .leading)
            headerCell("DLC", width: 68, alignment: .leading)
            headerCell("You've played", width: 170, alignment: .leading)
            headerCell("Last played", width: 136, alignment: .leading)
            headerCell("Region", width: 88, alignment: .leading)
            headerCell("Title ID", width: 170, alignment: .leading)
        }
        .padding(.vertical, 4)
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private func headerCell(_ title: String, width: CGFloat, alignment: Alignment) -> some View {
        Text(title)
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(.primary)
            .lineLimit(1)
            .frame(width: width, alignment: alignment)
            .padding(.horizontal, 8)
    }
}

struct GameListRowView: View {
    let game: GameItem
    let isSelected: Bool
    let isAlternateRow: Bool

    var body: some View {
        HStack(spacing: 0) {
            iconCell
                .frame(width: 66, alignment: .center)

            textCell(game.name, width: 340)
            textCell(game.version, width: 84)
            textCell(game.dlc, width: 68)
            textCell(game.played, width: 170)
            textCell(game.lastPlayed, width: 136)
            textCell(game.region, width: 88)
            textCell(String(format: "%016llx", game.titleID), width: 170)
        }
        .frame(maxWidth: .infinity, minHeight: 40, alignment: .leading)
        .background(backgroundColor)
    }

    private var iconCell: some View {
        RoundedRectangle(cornerRadius: 4)
            .fill(Color.accentColor.opacity(0.20))
            .frame(width: 40, height: 24)
            .overlay(
                Image(systemName: "photo")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundStyle(.secondary)
            )
            .padding(.horizontal, 8)
    }

    private func textCell(_ value: String, width: CGFloat) -> some View {
        Text(value)
            .font(.system(size: 12))
            .lineLimit(1)
            .truncationMode(.tail)
            .frame(width: width, alignment: .leading)
            .padding(.horizontal, 8)
    }

    private var backgroundColor: Color {
        if isSelected {
            return Color(nsColor: .selectedContentBackgroundColor)
        }

        if isAlternateRow {
            return Color(nsColor: .controlBackgroundColor)
        }

        return Color(nsColor: .windowBackgroundColor)
    }
}

struct GameListInfoBarView: View {
    let message: String
    let onDismiss: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "arrow.triangle.2.circlepath")
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(.secondary)

            Text(message)
                .font(.system(size: 12))

            Spacer()

            Button("Dismiss", action: onDismiss)
                .buttonStyle(.plain)
                .font(.system(size: 12, weight: .medium))
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 7)
        .background(Color(nsColor: .unemphasizedSelectedContentBackgroundColor))
    }
}

struct GameItem: Identifiable {
    let titleID: UInt64
    let name: String
    let version: String
    let dlc: String
    let played: String
    let lastPlayed: String
    let region: String

    var id: UInt64 { titleID }
}
