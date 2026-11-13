import AppKit
import Dispatch

@_cdecl("lyraSwiftSetWindowBackdropBlur")
public func lyraSwiftSetWindowBackdropBlur(
    _ opaqueWindow: UnsafeMutableRawPointer?,
    _ enabled: Int32
) -> Int32 {
    guard let opaqueWindow else { return 0 }
    let window = Unmanaged<NSWindow>.fromOpaque(opaqueWindow).takeUnretainedValue()

    let update = { () -> Int32 in
        guard let contentView = window.contentView else { return 0 }
        let identifier = NSUserInterfaceItemIdentifier("LyraWindowBackdropBlur")
        let existing = contentView.subviews.first { $0.identifier == identifier }

        if enabled == 0 {
            existing?.removeFromSuperview()
            window.isOpaque = true
            window.backgroundColor = .windowBackgroundColor
            return 1
        }
        if existing != nil { return 1 }

        let effectView = NSVisualEffectView(frame: contentView.bounds)
        effectView.identifier = identifier
        effectView.autoresizingMask = [.width, .height]
        effectView.material = .underWindowBackground
        effectView.blendingMode = .behindWindow
        effectView.state = .active

        // Keep the effect behind GLFW's content so Vulkan draws sharp UI above
        // the system-blurred desktop/windows visible through transparent pixels.
        contentView.addSubview(effectView, positioned: .below, relativeTo: nil)
        window.isOpaque = false
        window.backgroundColor = .clear
        return 1
    }

    if Thread.isMainThread { return update() }
    return DispatchQueue.main.sync(execute: update)
}
