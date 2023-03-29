#include "Cafe/HW/Latte/Renderer/Vulkan/CocoaSurface.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

@interface MetalView : NSView
@end

@implementation MetalView

-(BOOL) wantsUpdateLayer { return YES; }

+(Class) layerClass { return [CAMetalLayer class]; }

// copied from https://github.com/KhronosGroup/MoltenVK/blob/master/Demos/Cube/macOS/DemoViewController.m

-(CALayer*) makeBackingLayer
{
	CALayer* layer = [self.class.layerClass layer];
	CGSize viewScale = [self convertSizeToBacking: CGSizeMake(1.0, 1.0)];
	layer.contentsScale = MIN(viewScale.width, viewScale.height);
	return layer;
}

-(BOOL) layer: (CALayer *)layer shouldInheritContentsScale: (CGFloat)newScale fromWindow: (NSWindow *)window
{
	if (newScale == layer.contentsScale) { return NO; }

	layer.contentsScale = newScale;
	return YES;
}
@end

VkSurfaceKHR CreateCocoaSurface(VkInstance instance, void* handle)
{
	NSView* view = (NSView*)handle;

	MetalView* childView = [[MetalView alloc] initWithFrame:view.bounds];
	childView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	childView.wantsLayer = YES;

	[view addSubview:childView];

	VkMetalSurfaceCreateInfoEXT surface;
	surface.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	surface.pNext = NULL;
	surface.flags = 0;
	surface.pLayer = (CAMetalLayer*)childView.layer;

	VkSurfaceKHR result;
	VkResult err;
	if ((err = vkCreateMetalSurfaceEXT(instance, &surface, nullptr, &result)) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Cannot create a Metal Vulkan surface: {}", (sint32)err);
		throw std::runtime_error(fmt::format("Cannot create a Metal Vulkan surface: {}", err));
	}

	return result;
}
