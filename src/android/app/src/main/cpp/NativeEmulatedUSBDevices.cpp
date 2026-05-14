#include "Cafe/OS/libs/nsyshid/Infinity.h"
#include "Cafe/OS/libs/nsyshid/Skylander.h"
#include "Cafe/OS/libs/nsyshid/Dimensions.h"
#include "Cafe/OS/libs/nsyshid/nsyshid.h"
#include "config/ActiveSettings.h"

#include "JNIUtils.h"

namespace NativeEmulatedUSBDevices
{
	constexpr auto SKYLANDERS_FIGURES_PATH = "emulatedUSBDevices/skylanders";
	constexpr auto INFINITY_FIGURES_PATH = "emulatedUSBDevices/infinity";
	constexpr auto DIMENSIONS_FIGURES_PATH = "emulatedUSBDevices/dimensions";

	struct InfinityFigure
	{
		uint32 number;
		uint8 series;
		std::string name;
	};

	struct DimensionsMiniFigure
	{
		uint32 number;
		std::string name;
	};

	struct SkylanderFigure
	{
		uint8 portalSlot;
		uint16 id;
		uint16 variant;
		std::string name;
	};

	std::array<std::optional<SkylanderFigure>, nsyshid::MAX_SKYLANDERS> s_skylanderSlots;
	std::array<std::optional<InfinityFigure>, nsyshid::MAX_FIGURES> s_infinitySlots;
	std::array<std::optional<DimensionsMiniFigure>, 7> s_dimensionSlots;

	void ClearDimensionsFigure(uint8 pad, uint8 index)
	{
		s_dimensionSlots.at(index) = std::nullopt;
		nsyshid::g_dimensionstoypad.RemoveFigure(pad, index, true);
	}

	void ClearSkylandersFigure(uint8 slot)
	{
		if (auto& slotInfo = s_skylanderSlots.at(slot))
		{
			nsyshid::g_skyportal.RemoveSkylander(slotInfo->portalSlot);
			slotInfo = std::nullopt;
		}
	}

	void ClearInfinityFigure(uint8 slot)
	{
		s_infinitySlots.at(slot) = std::nullopt;
		nsyshid::g_infinitybase.RemoveFigure(slot);
	}

	jobjectArray GetInstalledFigures(JNIEnv* env, std::string_view figuresPath)
	{
		using namespace std::ranges::views;

		jclass figureClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeEmulatedUSBDevices$InstalledFigure");
		jmethodID ctrMID = env->GetMethodID(figureClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");

		std::error_code ec;
		auto installedFigures = fs::directory_iterator(ActiveSettings::GetUserDataPath(figuresPath), ec) |
								filter([&](const auto& e) { return e.is_regular_file(ec); }) |
								transform([](const auto& e) { return e.path(); });

		return JNIUtils::CreateObjectArray(
			env,
			figureClass,
			installedFigures,
			[&](auto&& figurePath) {
				return env->NewObject(
					figureClass,
					ctrMID,
					env->NewStringUTF(figurePath.stem().c_str()),
					env->NewStringUTF(figurePath.c_str()));
			});
	}

	template<size_t BufferSize>
	bool LoadFigure(const fs::path& path, std::invocable<std::array<uint8, BufferSize>&&, std::unique_ptr<FileStream>&&> auto f)
	{
		std::unique_ptr<FileStream> infFile(FileStream::openFile2(path, true));

		if (!infFile)
		{
			return false;
		}

		std::array<uint8, BufferSize> fileData{};
		if (infFile->readData(fileData.data(), fileData.size()) != fileData.size())
		{
			return false;
		}

		f(std::move(fileData), std::move(infFile));

		return true;
	}

	fs::path PrepareUniqueUserFilePath(std::string_view relativeDir, std::string_view fileName)
	{
		fs::path baseDir = ActiveSettings::GetUserDataPath(relativeDir);
		fs::path filePath = baseDir / fileName;

		std::error_code ec;
		fs::create_directories(filePath.parent_path(), ec);

		if (!fs::exists(filePath, ec))
		{
			return filePath;
		}

		std::string stem = filePath.stem().string();
		std::string ext = filePath.extension().string();

		int counter = 1;
		while (true)
		{
			fs::path newPath = baseDir / fmt::format("{} ({}){}", stem, counter, ext);
			if (!fs::exists(newPath, ec))
			{
				return newPath;
			}

			counter++;
		}
	}
} // namespace NativeEmulatedUSBDevices

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getInfinityFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	jclass infinityFigureClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeEmulatedUSBDevices$InfinityFigure");
	jmethodID ctrMID = env->GetMethodID(infinityFigureClass, "<init>", "(IBLjava/lang/String;)V");
	return JNIUtils::CreateObjectArray(
		env,
		infinityFigureClass,
		nsyshid::InfinityUSB::GetFigureList(),
		[&](auto&& figure) {
			const auto& [figureNumber, figureData] = figure;
			const auto& [figureSeries, figureName] = figureData;
			return env->NewObject(
				infinityFigureClass,
				ctrMID,
				static_cast<jint>(figureNumber),
				static_cast<jbyte>(figureSeries),
				env->NewStringUTF(figureName));
		});
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getDimensionsMiniFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	jclass dimensionMiniFigureClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeEmulatedUSBDevices$DimensionsMiniFigure");
	jmethodID ctrMID = env->GetMethodID(dimensionMiniFigureClass, "<init>", "(ILjava/lang/String;)V");
	return JNIUtils::CreateObjectArray(
		env,
		dimensionMiniFigureClass,
		nsyshid::DimensionsUSB::GetListMinifigs(),
		[&](auto&& figure) {
			const auto& [figureNumber, figureName] = figure;
			return env->NewObject(
				dimensionMiniFigureClass,
				ctrMID,
				static_cast<jint>(figureNumber),
				env->NewStringUTF(figureName));
		});
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getSkylanderFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	jclass skylanderClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeEmulatedUSBDevices$SkylanderFigure");
	jmethodID ctrMID = env->GetMethodID(skylanderClass, "<init>", "(SSLjava/lang/String;)V");
	return JNIUtils::CreateObjectArray(
		env,
		skylanderClass,
		nsyshid::SkylanderUSB::GetListSkylanders(),
		[&](auto&& skylander) {
			const auto& [skylanderNumber, skylanderName] = skylander;
			const auto& [skylanderId, skylanderVariant] = skylanderNumber;
			return env->NewObject(
				skylanderClass,
				ctrMID,
				static_cast<jshort>(skylanderId),
				static_cast<jshort>(skylanderVariant),
				env->NewStringUTF(skylanderName));
		});
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getInstalledDimensionsMiniFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NativeEmulatedUSBDevices::GetInstalledFigures(env, NativeEmulatedUSBDevices::DIMENSIONS_FIGURES_PATH);
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getInstalledInfinityFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NativeEmulatedUSBDevices::GetInstalledFigures(env, NativeEmulatedUSBDevices::INFINITY_FIGURES_PATH);
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getInstalledSkylanderFigures(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NativeEmulatedUSBDevices::GetInstalledFigures(env, NativeEmulatedUSBDevices::SKYLANDERS_FIGURES_PATH);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_deleteFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject installedFigure)
{
	jclass figureClass = env->GetObjectClass(installedFigure);
	jfieldID pathFieldId = env->GetFieldID(figureClass, "path", "Ljava/lang/String;");
	jstring pathJava = static_cast<jstring>(env->GetObjectField(installedFigure, pathFieldId));

	fs::path path = JNIUtils::FromJString(env, pathJava);

	if (std::error_code ec; !fs::remove(path, ec))
	{
		cemuLog_log(LogType::Force, "Failed to delete figure from {} {}", path.string(), ec.message());
		return false;
	}

	return true;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_createSkylanderFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jint id, jint variant)
{
	std::string skylanderFileName = nsyshid::g_skyportal.FindSkylander(id, variant) + ".sky";
	fs::path skylanderFilePath = NativeEmulatedUSBDevices::PrepareUniqueUserFilePath(NativeEmulatedUSBDevices::SKYLANDERS_FIGURES_PATH, skylanderFileName);
	return nsyshid::g_skyportal.CreateSkylander(skylanderFilePath, id, variant);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_createInfinityFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong number)
{
	auto [figureSeries, figureName] = nsyshid::g_infinitybase.FindFigure(number);
	std::string infinityFileName = figureName + ".bin";
	fs::path infinityFilePath = NativeEmulatedUSBDevices::PrepareUniqueUserFilePath(NativeEmulatedUSBDevices::INFINITY_FIGURES_PATH, infinityFileName);
	return nsyshid::g_infinitybase.CreateFigure(infinityFilePath, number, figureSeries);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_createDimensionsFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong number)
{
	std::string figureName = nsyshid::g_dimensionstoypad.FindFigure(number);
	std::string dimensionsFileName = figureName + ".bin";
	fs::path dimensionsFilePath = NativeEmulatedUSBDevices::PrepareUniqueUserFilePath(NativeEmulatedUSBDevices::DIMENSIONS_FIGURES_PATH, dimensionsFileName);
	return nsyshid::g_dimensionstoypad.CreateFigure(dimensionsFilePath, number);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_loadDimensionsFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring path, jint pad, jint index)
{
	return NativeEmulatedUSBDevices::LoadFigure<0x2D * 0x04>(JNIUtils::FromJString(env, path), [&](auto&& buffer, auto&& file) {
		NativeEmulatedUSBDevices::ClearDimensionsFigure(pad, index);

		uint32 figureNumber = nsyshid::g_dimensionstoypad.LoadFigure(buffer, std::forward<std::unique_ptr<FileStream>>(file), pad, index);
		std::string figureName = nsyshid::g_dimensionstoypad.FindFigure(figureNumber);
		NativeEmulatedUSBDevices::s_dimensionSlots.at(index) = NativeEmulatedUSBDevices::DimensionsMiniFigure{
			.number = figureNumber,
			.name = figureName,
		};
	});
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_loadInfinityFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring path, jint slot)
{
	return NativeEmulatedUSBDevices::LoadFigure<nsyshid::INF_FIGURE_SIZE>(JNIUtils::FromJString(env, path), [&](auto&& buffer, auto&& file) {
		NativeEmulatedUSBDevices::ClearInfinityFigure(slot);

		uint32 figureNumber = nsyshid::g_infinitybase.LoadFigure(buffer, std::forward<std::unique_ptr<FileStream>>(file), slot);
		auto [figureSeries, figureName] = nsyshid::g_infinitybase.FindFigure(figureNumber);
		NativeEmulatedUSBDevices::s_infinitySlots.at(slot) = NativeEmulatedUSBDevices::InfinityFigure{
			.number = figureNumber,
			.series = figureSeries,
			.name = figureName,
		};
	});
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_loadSkylandersFigure(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring path, jint slot)
{
	return NativeEmulatedUSBDevices::LoadFigure<nsyshid::SKY_FIGURE_SIZE>(JNIUtils::FromJString(env, path), [&](auto&& buffer, auto&& file) {
		NativeEmulatedUSBDevices::ClearSkylandersFigure(slot);

		uint16 skyId = uint16(buffer[0x11]) << 8 | uint16(buffer[0x10]);
		uint16 skyVar = uint16(buffer[0x1D]) << 8 | uint16(buffer[0x1C]);
		uint8 portalSlot = nsyshid::g_skyportal.LoadSkylander(buffer.data(), std::forward<std::unique_ptr<FileStream>>(file));

		NativeEmulatedUSBDevices::s_skylanderSlots.at(slot) = NativeEmulatedUSBDevices::SkylanderFigure{
			.portalSlot = portalSlot,
			.id = skyId,
			.variant = skyVar,
			.name = nsyshid::g_skyportal.FindSkylander(skyId, skyVar),
		};
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getSkylandersFigureSlot(JNIEnv* env, [[maybe_unused]] jclass clazz, jint slot)
{
	if (const auto& opt = NativeEmulatedUSBDevices::s_skylanderSlots.at(slot))
	{
		return env->NewStringUTF(opt->name.c_str());
	}

	return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getInfinityFigureSlot(JNIEnv* env, [[maybe_unused]] jclass clazz, jint slot)
{
	if (const auto& opt = NativeEmulatedUSBDevices::s_infinitySlots.at(slot))
	{
		return env->NewStringUTF(opt->name.c_str());
	}

	return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_getDimensionsFigureSlot(JNIEnv* env, [[maybe_unused]] jclass clazz, jint slot)
{
	if (const auto& opt = NativeEmulatedUSBDevices::s_dimensionSlots.at(slot))
	{
		return env->NewStringUTF(opt->name.c_str());
	}

	return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_clearDimensionsFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint pad, jint index)
{
	NativeEmulatedUSBDevices::ClearDimensionsFigure(pad, index);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_clearSkylandersFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint slot)
{
	NativeEmulatedUSBDevices::ClearSkylandersFigure(slot);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_clearInfinityFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint slot)
{
	NativeEmulatedUSBDevices::ClearInfinityFigure(slot);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_tempRemoveDimensionsFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index)
{
	nsyshid::g_dimensionstoypad.TempRemove(index);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_cancelRemoveDimensionsFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index)
{
	nsyshid::g_dimensionstoypad.CancelRemove(index);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeEmulatedUSBDevices_moveDimensionsFigure([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint newPad, jint newIndex, jint oldPad, jint oldIndex)
{
	nsyshid::g_dimensionstoypad.MoveFigure(newPad, newIndex, oldPad, oldIndex);
	if (newIndex != oldIndex)
	{
		auto& oldSlot = NativeEmulatedUSBDevices::s_dimensionSlots.at(oldIndex);
		NativeEmulatedUSBDevices::s_dimensionSlots.at(newIndex) = oldSlot;
		oldSlot = std::nullopt;
	}
}
