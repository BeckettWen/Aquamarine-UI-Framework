import AppKit
import Dispatch
import Metal
import QuartzCore

private final class LyraMetalView: NSView {
    override var isOpaque: Bool { false }
    override var wantsUpdateLayer: Bool { true }

    override func makeBackingLayer() -> CALayer { CAMetalLayer() }

    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    override func hitTest(_ point: NSPoint) -> NSView? {
        // GLFW remains responsible for mouse and keyboard event delivery.
        return nil
    }
}

private struct LyraViewportUniforms {
    var width: Float
    var height: Float
}

private struct LyraBlurUniforms {
    var width: Float
    var height: Float
    var sigma: Float
    var horizontal: UInt32
}

private final class LyraFrameResources {
    var rectangleVertices: MTLBuffer?
    var rectangleVertexCapacity = 0
    var rectangleIndices: MTLBuffer?
    var rectangleIndexCapacity = 0
    var textVertices: MTLBuffer?
    var textVertexCapacity = 0
    var textIndices: MTLBuffer?
    var textIndexCapacity = 0
}

private final class LyraMetalRenderer {
    let window: NSWindow
    let device: MTLDevice
    let commandQueue: MTLCommandQueue
    let view: LyraMetalView
    let drawableClearColor: MTLClearColor
    let sampler: MTLSamplerState
    let rectanglePipeline: MTLRenderPipelineState
    let textPipeline: MTLRenderPipelineState
    let blurPipeline: MTLRenderPipelineState
    let copyPipeline: MTLRenderPipelineState
    let inAppBlurEnabled: Bool
    let windowBlurView: NSVisualEffectView?
    private let inFlightFrames = DispatchSemaphore(value: 3)
    private let frameResources = [LyraFrameResources(), LyraFrameResources(),
                                  LyraFrameResources()]
    private var nextFrameResource = 0

    var textureArray: MTLTexture?
    var glyphArray: MTLTexture?
    var scene: MTLTexture?
    var blurScratch: MTLTexture?
    var blurred: MTLTexture?
    var targetWidth = 0
    var targetHeight = 0

    init?(window: NSWindow, width: Int, height: Int,
          windowBlurEnabled: Bool, inAppBlurEnabled: Bool,
          metallibPath: String) {
        guard let device = MTLCreateSystemDefaultDevice(),
              let queue = device.makeCommandQueue(),
              let library = try? device.makeLibrary(URL: URL(fileURLWithPath: metallibPath)),
              let vertex = library.makeFunction(name: "lyraVertex"),
              let rectangleFragment = library.makeFunction(name: "lyraRectangleFragment"),
              let textFragment = library.makeFunction(name: "lyraTextFragment"),
              let fullscreenVertex = library.makeFunction(name: "lyraFullscreenVertex"),
              let blurFragment = library.makeFunction(name: "lyraBlurFragment"),
              let copyFragment = library.makeFunction(name: "lyraCopyFragment") else {
            NSLog("Lyra Metal: device, command queue, or compiled Metal shader library unavailable")
            return nil
        }

        self.window = window
        self.device = device
        self.commandQueue = queue
        self.inAppBlurEnabled = inAppBlurEnabled

        let contentView = window.contentView ?? NSView(frame: .zero)
        if window.contentView == nil { window.contentView = contentView }
        let effect: NSVisualEffectView?
        if windowBlurEnabled {
            // Extend the content view beneath the native title bar so this
            // single AppKit backdrop supplies one continuous material across
            // both the title bar and the Metal client area.
            window.styleMask.insert(.fullSizeContentView)
            window.titlebarAppearsTransparent = true
            window.titleVisibility = .visible
            let effectView = NSVisualEffectView(frame: contentView.bounds)
            effectView.identifier = NSUserInterfaceItemIdentifier("LyraMetalWindowBackdrop")
            effectView.autoresizingMask = [.width, .height]
            effectView.material = .underWindowBackground
            effectView.blendingMode = .behindWindow
            effectView.state = .active
            contentView.addSubview(effectView, positioned: .below, relativeTo: nil)
            window.isOpaque = false
            window.backgroundColor = .clear
            effect = effectView
        } else {
            effect = nil
        }
        self.windowBlurView = effect

        let metalView = LyraMetalView(frame: contentView.bounds)
        metalView.autoresizingMask = [.width, .height]
        metalView.wantsLayer = true
        let metalLayer = metalView.metalLayer
        metalLayer.device = device
        metalLayer.pixelFormat = .bgra8Unorm
        metalLayer.framebufferOnly = false
        metalLayer.isOpaque = false
        metalLayer.presentsWithTransaction = false
        let clearColor = windowBlurEnabled
            ? MTLClearColor(red: 0, green: 0, blue: 0, alpha: 0)
            : MTLClearColor(red: 0.1, green: 0.1, blue: 0.1, alpha: 1)
        metalLayer.backgroundColor = CGColor(red: clearColor.red, green: clearColor.green,
                                             blue: clearColor.blue, alpha: clearColor.alpha)
        contentView.addSubview(metalView, positioned: .above, relativeTo: nil)
        self.view = metalView
        self.drawableClearColor = clearColor

        let samplerDescriptor = MTLSamplerDescriptor()
        samplerDescriptor.minFilter = .linear
        samplerDescriptor.magFilter = .linear
        samplerDescriptor.mipFilter = .notMipmapped
        samplerDescriptor.sAddressMode = .clampToEdge
        samplerDescriptor.tAddressMode = .clampToEdge
        guard let metalSampler = device.makeSamplerState(descriptor: samplerDescriptor) else {
            metalView.removeFromSuperview()
            return nil
        }
        self.sampler = metalSampler

        let vertexDescriptor = MTLVertexDescriptor()
        vertexDescriptor.attributes[0].format = .float2
        vertexDescriptor.attributes[0].offset = 0
        vertexDescriptor.attributes[0].bufferIndex = 0
        vertexDescriptor.attributes[1].format = .float4
        vertexDescriptor.attributes[1].offset = 2 * MemoryLayout<Float>.size
        vertexDescriptor.attributes[1].bufferIndex = 0
        vertexDescriptor.attributes[2].format = .float2
        vertexDescriptor.attributes[2].offset = 6 * MemoryLayout<Float>.size
        vertexDescriptor.attributes[2].bufferIndex = 0
        vertexDescriptor.attributes[3].format = .float
        vertexDescriptor.attributes[3].offset = 8 * MemoryLayout<Float>.size
        vertexDescriptor.attributes[3].bufferIndex = 0
        vertexDescriptor.layouts[0].stride = 9 * MemoryLayout<Float>.size
        vertexDescriptor.layouts[0].stepFunction = .perVertex
        vertexDescriptor.layouts[0].stepRate = 1

        func makePipeline(fragment: MTLFunction,
                          vertexDescriptor: MTLVertexDescriptor?,
                          blending: Bool) -> MTLRenderPipelineState? {
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertexDescriptor == nil ? fullscreenVertex : vertex
            descriptor.fragmentFunction = fragment
            descriptor.vertexDescriptor = vertexDescriptor
            descriptor.colorAttachments[0].pixelFormat = metalLayer.pixelFormat
            descriptor.colorAttachments[0].isBlendingEnabled = blending
            if blending {
                descriptor.colorAttachments[0].rgbBlendOperation = .add
                descriptor.colorAttachments[0].alphaBlendOperation = .add
                descriptor.colorAttachments[0].sourceRGBBlendFactor = .sourceAlpha
                descriptor.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
                descriptor.colorAttachments[0].sourceAlphaBlendFactor = .one
                descriptor.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha
            }
            return try? device.makeRenderPipelineState(descriptor: descriptor)
        }

        guard let rectangles = makePipeline(fragment: rectangleFragment,
                                            vertexDescriptor: vertexDescriptor,
                                            blending: true),
              let text = makePipeline(fragment: textFragment,
                                      vertexDescriptor: vertexDescriptor,
                                      blending: true),
              let blur = makePipeline(fragment: blurFragment,
                                      vertexDescriptor: nil,
                                      blending: false),
              let copy = makePipeline(fragment: copyFragment,
                                      vertexDescriptor: nil,
                                      blending: false) else {
            metalView.removeFromSuperview()
            effect?.removeFromSuperview()
            return nil
        }
        self.rectanglePipeline = rectangles
        self.textPipeline = text
        self.blurPipeline = blur
        self.copyPipeline = copy
        metalLayer.drawableSize = CGSize(width: max(width, 1), height: max(height, 1))

        let whiteDescriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .rgba8Unorm, width: 1, height: 1, mipmapped: false)
        whiteDescriptor.textureType = .type2DArray
        whiteDescriptor.arrayLength = 1
        whiteDescriptor.storageMode = .shared
        whiteDescriptor.usage = .shaderRead
        guard let whiteTexture = device.makeTexture(descriptor: whiteDescriptor) else {
            metalView.removeFromSuperview()
            effect?.removeFromSuperview()
            return nil
        }
        let white: [UInt8] = [255, 255, 255, 255]
        white.withUnsafeBytes { bytes in
            if let baseAddress = bytes.baseAddress {
                whiteTexture.replace(region: MTLRegionMake2D(0, 0, 1, 1),
                                     mipmapLevel: 0, slice: 0,
                                     withBytes: baseAddress, bytesPerRow: 4,
                                     bytesPerImage: 4)
            }
        }
        self.textureArray = whiteTexture
        if effect != nil {
            NSLog("Lyra Metal: unified AppKit backdrop attached under title bar and window content")
        }
    }

    func resize(width: Int, height: Int) {
        view.metalLayer.drawableSize = CGSize(width: max(width, 1), height: max(height, 1))
    }

    func uploadArray(bytes: UnsafeRawPointer?, width: Int, height: Int,
                     layers: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        guard let bytes, width > 0, height > 0, layers > 0 else { return nil }
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: pixelFormat, width: width, height: height,
            mipmapped: false)
        descriptor.textureType = .type2DArray
        descriptor.arrayLength = layers
        descriptor.storageMode = .shared
        descriptor.usage = .shaderRead
        guard let texture = device.makeTexture(descriptor: descriptor) else { return nil }
        let bytesPerPixel = pixelFormat == .r8Unorm ? 1 : 4
        let layerBytes = width * height * bytesPerPixel
        for layer in 0..<layers {
            texture.replace(
                region: MTLRegionMake2D(0, 0, width, height),
                mipmapLevel: 0,
                slice: layer,
                withBytes: bytes.advanced(by: layer * layerBytes),
                bytesPerRow: width * bytesPerPixel,
                bytesPerImage: layerBytes)
        }
        return texture
    }

    func ensureTargets(width: Int, height: Int) -> Bool {
        let size = view.metalLayer.drawableSize
        let targetW = max(width > 0 ? width : Int(size.width), 1)
        let targetH = max(height > 0 ? height : Int(size.height), 1)
        guard targetW != targetWidth || targetH != targetHeight else { return true }

        let descriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: view.metalLayer.pixelFormat, width: targetW,
            height: targetH, mipmapped: false)
        descriptor.storageMode = .private
        descriptor.usage = [.renderTarget, .shaderRead]
        guard let scene = device.makeTexture(descriptor: descriptor),
              let scratch = device.makeTexture(descriptor: descriptor),
              let blurred = device.makeTexture(descriptor: descriptor) else {
            return false
        }
        self.scene = scene
        self.blurScratch = scratch
        self.blurred = blurred
        targetWidth = targetW
        targetHeight = targetH
        return true
    }

    private func ensureBuffer(_ buffer: inout MTLBuffer?,
                              capacity: inout Int,
                              byteCount: Int) -> Bool {
        guard byteCount >= 0 else { return false }
        guard byteCount > capacity else { return true }
        var newCapacity = 256
        while newCapacity < byteCount {
            guard newCapacity <= Int.max / 2 else {
                newCapacity = byteCount
                break
            }
            newCapacity *= 2
        }
        guard let newBuffer = device.makeBuffer(length: newCapacity,
                                                options: .storageModeShared) else {
            return false
        }
        buffer = newBuffer
        capacity = newCapacity
        return true
    }

    private func copy(_ source: UnsafeRawPointer?, byteCount: Int,
                      to buffer: MTLBuffer?) -> Bool {
        guard byteCount >= 0 else { return false }
        guard byteCount > 0 else { return true }
        guard let source, let buffer, buffer.length >= byteCount else { return false }
        buffer.contents().copyMemory(from: source, byteCount: byteCount)
        return true
    }

    private func scissor(_ x: Float, _ y: Float, _ width: Float, _ height: Float,
                         scaleX: Float, scaleY: Float) -> MTLScissorRect? {
        let left = max(Int(floor(x * scaleX)), 0)
        let top = max(Int(floor(y * scaleY)), 0)
        let right = min(Int(ceil((x + width) * scaleX)), targetWidth)
        let bottom = min(Int(ceil((y + height) * scaleY)), targetHeight)
        guard right > left, bottom > top else { return nil }
        return MTLScissorRect(x: left, y: top, width: right - left, height: bottom - top)
    }

    private func encodeBlur(commandBuffer: MTLCommandBuffer,
                            source: MTLTexture, destination: MTLTexture,
                            sigma: Float, horizontal: Bool,
                            scissorRect: MTLScissorRect) -> Bool {
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = destination
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        guard let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else {
            return false
        }
        encoder.setRenderPipelineState(blurPipeline)
        encoder.setFragmentTexture(source, index: 0)
        encoder.setFragmentSamplerState(sampler, index: 0)
        var uniforms = LyraBlurUniforms(width: Float(targetWidth),
                                        height: Float(targetHeight),
                                        sigma: sigma,
                                        horizontal: horizontal ? 1 : 0)
        encoder.setFragmentBytes(&uniforms, length: MemoryLayout<LyraBlurUniforms>.stride,
                                 index: 0)
        encoder.setViewport(MTLViewport(originX: 0, originY: 0,
                                        width: Double(targetWidth),
                                        height: Double(targetHeight),
                                        znear: 0, zfar: 1))
        encoder.setScissorRect(scissorRect)
        encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        encoder.endEncoding()
        return true
    }

    private func encodeBackdrop(commandBuffer: MTLCommandBuffer,
                                source: MTLTexture, destination: MTLTexture,
                                region: MTLScissorRect) -> Bool {
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = destination
        pass.colorAttachments[0].loadAction = .load
        pass.colorAttachments[0].storeAction = .store
        guard let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else {
            return false
        }
        encoder.setRenderPipelineState(copyPipeline)
        encoder.setFragmentTexture(source, index: 0)
        encoder.setFragmentSamplerState(sampler, index: 0)
        encoder.setViewport(MTLViewport(originX: 0, originY: 0,
                                        width: Double(targetWidth),
                                        height: Double(targetHeight),
                                        znear: 0, zfar: 1))
        encoder.setScissorRect(region)
        encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        encoder.endEncoding()
        return true
    }

    private func applyBlur(commandBuffer: MTLCommandBuffer,
                           region: MTLScissorRect, radius: Float,
                           scaleX: Float, scaleY: Float) -> Bool {
        guard radius.isFinite, radius > 0,
              let scene, let blurScratch, let blurred else { return true }
        let sigmaX = min(max(radius * scaleX, 0.5), 64)
        let sigmaY = min(max(radius * scaleY, 0.5), 64)
        let verticalPad = Int(ceil(sigmaY * 2))
        let horizontalRegion = MTLScissorRect(
            x: region.x,
            y: max(region.y - verticalPad, 0),
            width: region.width,
            height: min(targetHeight, region.y + region.height + verticalPad) -
                    max(region.y - verticalPad, 0))
        return encodeBlur(commandBuffer: commandBuffer, source: scene,
                          destination: blurScratch, sigma: sigmaX,
                          horizontal: true, scissorRect: horizontalRegion) &&
               encodeBlur(commandBuffer: commandBuffer, source: blurScratch,
                          destination: blurred, sigma: sigmaY,
                          horizontal: false, scissorRect: region) &&
               encodeBackdrop(commandBuffer: commandBuffer, source: blurred,
                              destination: scene, region: region)
    }

    func render(rectangleVertices: UnsafeRawPointer?,
                rectangleEffects: UnsafeRawPointer?, rectangleCount: Int,
                textVertices: UnsafeRawPointer?, textVertexCount: Int,
                textIndices: UnsafeRawPointer?, textIndexCount: Int,
                textDraws: UnsafeRawPointer?, textDrawCount: Int,
                framebufferWidth: Int, framebufferHeight: Int,
                windowWidth: Int, windowHeight: Int) -> Bool {
        guard rectangleCount >= 0,
              rectangleCount <= Int(UInt32.max / 4),
              textVertexCount >= 0, textIndexCount >= 0,
              textDrawCount >= 0,
              (rectangleCount == 0 || (rectangleVertices != nil && rectangleEffects != nil)),
              (textVertexCount == 0 || textVertices != nil),
              (textIndexCount == 0 || textIndices != nil),
              (textDrawCount == 0 || textDraws != nil),
              ensureTargets(width: framebufferWidth, height: framebufferHeight),
              let scene else { return false }

        inFlightFrames.wait()
        var committed = false
        defer {
            if !committed { inFlightFrames.signal() }
        }
        guard let drawable = view.metalLayer.nextDrawable(),
              let commandBuffer = commandQueue.makeCommandBuffer() else { return false }
        let frame = frameResources[nextFrameResource]
        nextFrameResource = (nextFrameResource + 1) % frameResources.count

        let rectangleVertexBytes = rectangleCount * 36 * MemoryLayout<Float>.size
        let rectangleIndexCount = rectangleCount * 6
        let rectangleIndexBytes = rectangleIndexCount * MemoryLayout<UInt32>.size
        let textVertexBytes = textVertexCount * 9 * MemoryLayout<Float>.size
        let textIndexBytes = textIndexCount * MemoryLayout<UInt32>.size
        guard ensureBuffer(&frame.rectangleVertices,
                           capacity: &frame.rectangleVertexCapacity,
                           byteCount: rectangleVertexBytes),
              ensureBuffer(&frame.rectangleIndices,
                           capacity: &frame.rectangleIndexCapacity,
                           byteCount: rectangleIndexBytes),
              ensureBuffer(&frame.textVertices,
                           capacity: &frame.textVertexCapacity,
                           byteCount: textVertexBytes),
              ensureBuffer(&frame.textIndices,
                           capacity: &frame.textIndexCapacity,
                           byteCount: textIndexBytes),
              copy(rectangleVertices, byteCount: rectangleVertexBytes,
                   to: frame.rectangleVertices),
              copy(textVertices, byteCount: textVertexBytes, to: frame.textVertices),
              copy(textIndices, byteCount: textIndexBytes, to: frame.textIndices) else {
            return false
        }

        if rectangleCount > 0, let rectangleIndexBuffer = frame.rectangleIndices {
            let indices = rectangleIndexBuffer.contents().assumingMemoryBound(to: UInt32.self)
            for rectangle in 0..<rectangleCount {
                let vertex = UInt32(rectangle * 4)
                let index = rectangle * 6
                indices[index + 0] = vertex
                indices[index + 1] = vertex + 1
                indices[index + 2] = vertex + 2
                indices[index + 3] = vertex
                indices[index + 4] = vertex + 2
                indices[index + 5] = vertex + 3
            }
        }

        commandBuffer.addCompletedHandler { [inFlightFrames] _ in
            inFlightFrames.signal()
        }

        let scaleX = Float(targetWidth) / Float(max(windowWidth, 1))
        let scaleY = Float(targetHeight) / Float(max(windowHeight, 1))
        let effects = rectangleCount > 0
            ? UnsafeBufferPointer(start: rectangleEffects!.assumingMemoryBound(to: Float.self),
                                  count: rectangleCount * 6)
            : UnsafeBufferPointer<Float>(start: nil, count: 0)
        let textMetadata = textDrawCount > 0
            ? UnsafeBufferPointer(start: textDraws!.assumingMemoryBound(to: Float.self),
                                  count: textDrawCount * 8)
            : UnsafeBufferPointer<Float>(start: nil, count: 0)

        let scenePass = MTLRenderPassDescriptor()
        scenePass.colorAttachments[0].texture = scene
        scenePass.colorAttachments[0].loadAction = .clear
        scenePass.colorAttachments[0].storeAction = .store
        scenePass.colorAttachments[0].clearColor = drawableClearColor
        guard let sceneEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: scenePass) else {
            return false
        }
        var encoder: MTLRenderCommandEncoder? = sceneEncoder
        encoder?.setViewport(MTLViewport(originX: 0, originY: 0,
                                         width: Double(targetWidth),
                                         height: Double(targetHeight),
                                         znear: 0, zfar: 1))
        var viewportUniforms = LyraViewportUniforms(width: Float(max(windowWidth, 1)),
                                                     height: Float(max(windowHeight, 1)))

        var rectangleIndex = 0
        while rectangleIndex < rectangleCount {
            let effectOffset = rectangleIndex * 6
            let blurEnabled = effects[effectOffset] > 0.5 && inAppBlurEnabled
            if blurEnabled,
               let region = scissor(effects[effectOffset + 2], effects[effectOffset + 3],
                                    effects[effectOffset + 4], effects[effectOffset + 5],
                                    scaleX: scaleX, scaleY: scaleY) {
                encoder?.endEncoding()
                encoder = nil
                guard applyBlur(commandBuffer: commandBuffer, region: region,
                                radius: effects[effectOffset + 1],
                                scaleX: scaleX, scaleY: scaleY) else { return false }
                let loadPass = MTLRenderPassDescriptor()
                loadPass.colorAttachments[0].texture = scene
                loadPass.colorAttachments[0].loadAction = .load
                loadPass.colorAttachments[0].storeAction = .store
                guard let nextEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: loadPass) else {
                    return false
                }
                encoder = nextEncoder
                encoder?.setViewport(MTLViewport(originX: 0, originY: 0,
                                                 width: Double(targetWidth),
                                                 height: Double(targetHeight),
                                                 znear: 0, zfar: 1))
            }

            let runStart = rectangleIndex
            if !blurEnabled {
                rectangleIndex += 1
                while rectangleIndex < rectangleCount &&
                      !(effects[rectangleIndex * 6] > 0.5 && inAppBlurEnabled) {
                    rectangleIndex += 1
                }
            } else {
                rectangleIndex += 1
            }
            let runCount = rectangleIndex - runStart
            guard runCount > 0,
                  let rectangleVertexBuffer = frame.rectangleVertices,
                  let rectangleIndexBuffer = frame.rectangleIndices else { return false }
            encoder?.setRenderPipelineState(rectanglePipeline)
            encoder?.setVertexBuffer(rectangleVertexBuffer, offset: 0, index: 0)
            encoder?.setVertexBytes(&viewportUniforms,
                                    length: MemoryLayout<LyraViewportUniforms>.stride,
                                    index: 1)
            if let textureArray {
                encoder?.setFragmentTexture(textureArray, index: 0)
                encoder?.setFragmentSamplerState(sampler, index: 0)
            }
            encoder?.drawIndexedPrimitives(
                type: .triangle, indexCount: runCount * 6, indexType: .uint32,
                indexBuffer: rectangleIndexBuffer,
                indexBufferOffset: runStart * 6 * MemoryLayout<UInt32>.size)
        }

        if textDrawCount > 0, let textVertexBuffer = frame.textVertices,
           let textIndexBuffer = frame.textIndices,
           let glyphArray {
            for drawIndex in 0..<textDrawCount {
                let base = drawIndex * 8
                let firstIndex = max(Int(textMetadata[base]), 0)
                let indexCount = max(Int(textMetadata[base + 1]), 0)
                guard indexCount > 0 else { continue }
                if textMetadata[base + 2] > 0.5,
                   let region = scissor(textMetadata[base + 4], textMetadata[base + 5],
                                        textMetadata[base + 6], textMetadata[base + 7],
                                        scaleX: scaleX, scaleY: scaleY) {
                    encoder?.endEncoding()
                    encoder = nil
                    guard applyBlur(commandBuffer: commandBuffer, region: region,
                                    radius: textMetadata[base + 3],
                                    scaleX: scaleX, scaleY: scaleY) else { return false }
                    let loadPass = MTLRenderPassDescriptor()
                    loadPass.colorAttachments[0].texture = scene
                    loadPass.colorAttachments[0].loadAction = .load
                    loadPass.colorAttachments[0].storeAction = .store
                    guard let nextEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: loadPass) else {
                        return false
                    }
                    encoder = nextEncoder
                    encoder?.setViewport(MTLViewport(originX: 0, originY: 0,
                                                     width: Double(targetWidth),
                                                     height: Double(targetHeight),
                                                     znear: 0, zfar: 1))
                }
                encoder?.setRenderPipelineState(textPipeline)
                encoder?.setVertexBuffer(textVertexBuffer, offset: 0, index: 0)
                encoder?.setVertexBytes(&viewportUniforms,
                                        length: MemoryLayout<LyraViewportUniforms>.stride,
                                        index: 1)
                encoder?.setFragmentTexture(glyphArray, index: 0)
                encoder?.setFragmentSamplerState(sampler, index: 0)
                encoder?.drawIndexedPrimitives(
                    type: .triangle, indexCount: indexCount, indexType: .uint32,
                    indexBuffer: textIndexBuffer,
                    indexBufferOffset: firstIndex * MemoryLayout<UInt32>.size)
            }
        }
        encoder?.endEncoding()

        let drawablePass = MTLRenderPassDescriptor()
        drawablePass.colorAttachments[0].texture = drawable.texture
        drawablePass.colorAttachments[0].loadAction = .clear
        drawablePass.colorAttachments[0].storeAction = .store
        drawablePass.colorAttachments[0].clearColor = drawableClearColor
        guard let drawableEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: drawablePass) else {
            return false
        }
        drawableEncoder.setRenderPipelineState(copyPipeline)
        drawableEncoder.setFragmentTexture(scene, index: 0)
        drawableEncoder.setFragmentSamplerState(sampler, index: 0)
        drawableEncoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        drawableEncoder.endEncoding()
        commandBuffer.present(drawable)
        committed = true
        commandBuffer.commit()
        return true
    }
}

@_cdecl("lyraSwiftMetalCreate")
public func lyraSwiftMetalCreate(_ nativeWindow: UnsafeMutableRawPointer?,
                                 _ width: Int32, _ height: Int32,
                                 _ windowBlurEnabled: Int32,
                                 _ inAppBlurEnabled: Int32,
                                 _ metallibPath: UnsafePointer<CChar>?) -> UnsafeMutableRawPointer? {
    guard let nativeWindow, let metallibPath else { return nil }
    let window = Unmanaged<NSWindow>.fromOpaque(nativeWindow).takeUnretainedValue()
    guard let renderer = LyraMetalRenderer(
        window: window, width: Int(width), height: Int(height),
        windowBlurEnabled: windowBlurEnabled != 0,
        inAppBlurEnabled: inAppBlurEnabled != 0,
        metallibPath: String(cString: metallibPath)) else { return nil }
    return Unmanaged.passRetained(renderer).toOpaque()
}

@_cdecl("lyraMetalDestroy")
public func lyraMetalDestroy(_ opaqueRenderer: UnsafeMutableRawPointer?) {
    guard let opaqueRenderer else { return }
    let renderer = Unmanaged<LyraMetalRenderer>.fromOpaque(opaqueRenderer).takeRetainedValue()
    renderer.view.removeFromSuperview()
    renderer.windowBlurView?.removeFromSuperview()
    if renderer.windowBlurView != nil {
        renderer.window.isOpaque = true
        renderer.window.backgroundColor = .windowBackgroundColor
    }
}

@_cdecl("lyraMetalResize")
public func lyraMetalResize(_ opaqueRenderer: UnsafeMutableRawPointer?,
                            _ width: Int32, _ height: Int32) {
    guard let opaqueRenderer else { return }
    Unmanaged<LyraMetalRenderer>.fromOpaque(opaqueRenderer).takeUnretainedValue()
        .resize(width: Int(width), height: Int(height))
}

@_cdecl("lyraMetalUploadTextures")
public func lyraMetalUploadTextures(_ opaqueRenderer: UnsafeMutableRawPointer?,
                                    _ bytes: UnsafeRawPointer?,
                                    _ width: Int32, _ height: Int32,
                                    _ layers: Int32) -> Int32 {
    guard let opaqueRenderer else { return 0 }
    let renderer = Unmanaged<LyraMetalRenderer>.fromOpaque(opaqueRenderer).takeUnretainedValue()
    guard let texture = renderer.uploadArray(bytes: bytes, width: Int(width),
                                             height: Int(height), layers: Int(layers),
                                             pixelFormat: .rgba8Unorm) else { return 0 }
    renderer.textureArray = texture
    NSLog("Lyra Metal: uploaded %d texture layer(s)", layers)
    return 1
}

@_cdecl("lyraMetalUploadGlyphAtlas")
public func lyraMetalUploadGlyphAtlas(_ opaqueRenderer: UnsafeMutableRawPointer?,
                                      _ bytes: UnsafeRawPointer?,
                                      _ width: Int32, _ height: Int32,
                                      _ layers: Int32) -> Int32 {
    guard let opaqueRenderer else { return 0 }
    let renderer = Unmanaged<LyraMetalRenderer>.fromOpaque(opaqueRenderer).takeUnretainedValue()
    guard let texture = renderer.uploadArray(bytes: bytes, width: Int(width),
                                             height: Int(height), layers: Int(layers),
                                             pixelFormat: .r8Unorm) else { return 0 }
    renderer.glyphArray = texture
    return 1
}

@_cdecl("lyraMetalRender")
public func lyraMetalRender(_ opaqueRenderer: UnsafeMutableRawPointer?,
                            _ rectangleVertices: UnsafeRawPointer?,
                            _ rectangleEffects: UnsafeRawPointer?,
                            _ rectangleCount: Int32,
                            _ textVertices: UnsafeRawPointer?,
                            _ textVertexCount: Int32,
                            _ textIndices: UnsafeRawPointer?,
                            _ textIndexCount: Int32,
                            _ textDraws: UnsafeRawPointer?,
                            _ textDrawCount: Int32,
                            _ framebufferWidth: Int32, _ framebufferHeight: Int32,
                            _ windowWidth: Int32, _ windowHeight: Int32) -> Int32 {
    guard let opaqueRenderer else { return 0 }
    let renderer = Unmanaged<LyraMetalRenderer>.fromOpaque(opaqueRenderer).takeUnretainedValue()
    return renderer.render(
        rectangleVertices: rectangleVertices,
        rectangleEffects: rectangleEffects,
        rectangleCount: Int(rectangleCount),
        textVertices: textVertices,
        textVertexCount: Int(textVertexCount),
        textIndices: textIndices,
        textIndexCount: Int(textIndexCount),
        textDraws: textDraws,
        textDrawCount: Int(textDrawCount),
        framebufferWidth: Int(framebufferWidth),
        framebufferHeight: Int(framebufferHeight),
        windowWidth: Int(windowWidth),
        windowHeight: Int(windowHeight)) ? 1 : 0
}
