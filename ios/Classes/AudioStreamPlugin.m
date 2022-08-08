#import "AudioStreamPlugin.h"
#if __has_include(<audio_stream/audio_stream-Swift.h>)
#import <audio_stream/audio_stream-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "audio_stream-Swift.h"
#endif

@implementation AudioStreamPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftAudioStreamPlugin registerWithRegistrar:registrar];
}
@end
