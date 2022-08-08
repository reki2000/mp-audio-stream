import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import '../audio_stream.dart';

typedef _MAInitFunc = Int Function();
typedef _MAInit = int Function();

typedef _MAPushFunc = Void Function(Pointer<Float>, Int64);
typedef _MAPush = void Function(Pointer<Float>, int);

typedef _MAUninitFunc = Void Function();
typedef _MAUninit = void Function();

class AudioStreamImpl implements AudioStream {
  late _MAPush _pushFfi;
  late _MAUninit _uninitFfi;

  @override
  Future<int> init() async {
    final dynLib = (Platform.isLinux || Platform.isAndroid)
        ? DynamicLibrary.open("libaudio_stream.so")
        : Platform.isWindows
            ? DynamicLibrary.open("audio_stream.dll")
            : (Platform.isMacOS || Platform.isIOS)
                ? DynamicLibrary.executable()
                : DynamicLibrary.executable();

    final initFfi = dynLib
        .lookup<NativeFunction<_MAInitFunc>>("ma_stream_init")
        .asFunction<_MAInit>();

    _pushFfi = dynLib
        .lookup<NativeFunction<_MAPushFunc>>("ma_stream_push")
        .asFunction<_MAPush>();

    _uninitFfi = dynLib
        .lookup<NativeFunction<_MAUninitFunc>>("ma_stream_uninit")
        .asFunction<_MAUninit>();

    return initFfi();
  }

  @override
  Future<void> push(Float32List buf) async {
    final ffiBuf = calloc<Float>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    _pushFfi(ffiBuf, buf.length);
    calloc.free(ffiBuf);
  }

  @override
  Future<void> uninit() async {
    _uninitFfi();
  }
}
