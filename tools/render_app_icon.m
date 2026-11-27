// Render a custom NEIN app icon: center-crop any ImageIO-readable picture to
// a square, flatten it onto white (home screen icons must be opaque) and
// write one PNG per requested pixel size.
//
// usage: render_app_icon INPUT OUTPUT-DIRECTORY FILENAME=SIZE...
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

static CGImageRef NEINCreateOrientedImage(NSURL *url) {
    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!source) return NULL;
    CGImageRef image = NULL;
    NSDictionary *properties = CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(source, 0, NULL));
    NSNumber *width = properties[(__bridge NSString *)kCGImagePropertyPixelWidth];
    NSNumber *height = properties[(__bridge NSString *)kCGImagePropertyPixelHeight];
    if (width && height) {
        // A full-size thumbnail applies the EXIF orientation for us.
        NSDictionary *options = @{
            (__bridge NSString *)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
            (__bridge NSString *)kCGImageSourceCreateThumbnailWithTransform: @YES,
            (__bridge NSString *)kCGImageSourceThumbnailMaxPixelSize: @(MAX(width.integerValue, height.integerValue)),
        };
        image = CGImageSourceCreateThumbnailAtIndex(source, 0, (__bridge CFDictionaryRef)options);
    }
    CFRelease(source);
    return image;
}

static BOOL NEINWriteIcon(CGImageRef square, size_t size, NSURL *url) {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (!colorSpace) return NO;
    CGContextRef context = CGBitmapContextCreate(
        NULL, size, size, 8, size * 4, colorSpace,
        kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);
    if (!context) return NO;
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, CGRectMake(0, 0, size, size));
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
    CGContextDrawImage(context, CGRectMake(0, 0, size, size), square);
    CGImageRef icon = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    if (!icon) return NO;

    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)url, (__bridge CFStringRef)UTTypePNG.identifier, 1, NULL
    );
    BOOL written = NO;
    if (destination) {
        CGImageDestinationAddImage(destination, icon, NULL);
        written = CGImageDestinationFinalize(destination);
        CFRelease(destination);
    }
    CGImageRelease(icon);
    return written;
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc < 4) {
            fprintf(stderr, "usage: render_app_icon INPUT OUTPUT-DIRECTORY FILENAME=SIZE...\n");
            return 2;
        }
        NSURL *input = [NSURL fileURLWithPath:@(argv[1])];
        NSURL *output = [NSURL fileURLWithPath:@(argv[2]) isDirectory:YES];

        NSMutableArray<NSString *> *names = [NSMutableArray array];
        NSMutableArray<NSNumber *> *sizes = [NSMutableArray array];
        size_t largest = 0;
        for (int index = 3; index < argc; index++) {
            NSArray<NSString *> *parts = [@(argv[index]) componentsSeparatedByString:@"="];
            NSInteger size = parts.count == 2 ? parts[1].integerValue : 0;
            if (size <= 0 || parts[0].length == 0 ||
                    ![parts[0].lastPathComponent isEqualToString:parts[0]]) {
                fprintf(stderr, "Invalid icon request: %s\n", argv[index]);
                return 2;
            }
            [names addObject:parts[0]];
            [sizes addObject:@(size)];
            largest = MAX(largest, (size_t)size);
        }

        CGImageRef image = NEINCreateOrientedImage(input);
        if (!image) {
            fprintf(stderr, "Could not read the icon image.\n");
            return 1;
        }
        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        size_t side = MIN(width, height);
        if (side < largest) {
            fprintf(stderr, "The icon image is %zux%zu; its shorter side must be at least %zu pixels.\n",
                    width, height, largest);
            CGImageRelease(image);
            return 1;
        }
        CGImageRef square = CGImageCreateWithImageInRect(
            image, CGRectMake((width - side) / 2, (height - side) / 2, side, side)
        );
        CGImageRelease(image);
        if (!square) {
            fprintf(stderr, "Could not crop the icon image.\n");
            return 1;
        }

        int status = 0;
        for (NSUInteger index = 0; index < names.count; index++) {
            NSURL *url = [output URLByAppendingPathComponent:names[index]];
            if (!NEINWriteIcon(square, sizes[index].unsignedLongValue, url)) {
                fprintf(stderr, "Could not write %s.\n", names[index].UTF8String);
                status = 1;
                break;
            }
        }
        CGImageRelease(square);
        return status;
    }
}
