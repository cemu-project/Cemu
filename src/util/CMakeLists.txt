add_library(Util
  boost/bluetooth.h
  ChunkedHeap/ChunkedHeap.h
  containers/flat_hash_map.hpp
  containers/IntervalBucketContainer.h
  containers/LookupTableL3.h
  containers/RangeStore.h
  containers/robin_hood.h
  containers/SmallBitset.h
  crypto/aes128.cpp
  crypto/aes128.h
  crypto/crc32.cpp
  crypto/crc32.h
  crypto/md5.cpp
  crypto/md5.h
  DXGIWrapper/DXGIWrapper.h
  EventService.h
  Fiber/Fiber.h
  Fiber/FiberUnix.cpp
  Fiber/FiberWin.cpp
  helpers/ClassWrapper.h
  helpers/ConcurrentQueue.h
  helpers/enum_array.hpp
  helpers/fixedSizeList.h
  helpers/fspinlock.h
  helpers/helpers.cpp
  helpers/helpers.h
  helpers/MapAdaptor.h
  helpers/MemoryPool.h
  helpers/ringbuffer.h
  helpers/Semaphore.h
  helpers/Serializer.h
  helpers/Singleton.h
  helpers/StringBuf.h
  helpers/StringHelpers.h
  helpers/StringParser.h
  helpers/SystemException.h
  helpers/TempState.h
  highresolutiontimer/HighResolutionTimer.cpp
  highresolutiontimer/HighResolutionTimer.h
  ImageWriter/bmp.h
  ImageWriter/tga.h
  IniParser/IniParser.cpp
  IniParser/IniParser.h
  libusbWrapper/libusbWrapper.cpp
  libusbWrapper/libusbWrapper.h
  math/glm.h
  math/quaternion.h
  math/vector2.h
  math/vector3.h
  MemMapper/MemMapper.h
  MemMapper/MemMapperUnix.cpp
  MemMapper/MemMapperWin.cpp
  ThreadPool/ThreadPool.cpp
  ThreadPool/ThreadPool.h
  tinyxml2/tinyxml2.cpp
  tinyxml2/tinyxml2.h
  VirtualHeap/VirtualHeap.cpp
  VirtualHeap/VirtualHeap.h
  Zir/Core/IR.cpp
  Zir/Core/IR.h
  Zir/Core/ZirUtility.h
  Zir/Core/ZpIRBuilder.h
  Zir/Core/ZpIRDebug.h
  Zir/Core/ZpIRPasses.h
  Zir/Core/ZpIRScheduler.h
  Zir/EmitterGLSL/ZpIREmitGLSL.cpp
  Zir/EmitterGLSL/ZpIREmitGLSL.h
  Zir/Passes/RegisterAllocatorForGLSL.cpp
  Zir/Passes/ZpIRRegisterAllocator.cpp
)

target_precompile_headers(Util PRIVATE ../Common/precompiled.h)

target_link_libraries(Util PRIVATE Common Config)

target_link_libraries(Util PRIVATE Boost::headers Boost::nowide OpenSSL::Crypto fmt::fmt)

target_include_directories(Common PRIVATE ${fmt_INCLUDE_DIRS})

if (ENABLE_WXWIDGETS)
  target_link_libraries(Util PRIVATE wx::base wx::core)
endif()
