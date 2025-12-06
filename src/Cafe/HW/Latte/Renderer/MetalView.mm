#include "Cafe/HW/Latte/Renderer/MetalView.h"

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
