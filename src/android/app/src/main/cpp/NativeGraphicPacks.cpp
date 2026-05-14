#include "Cafe/CafeSystem.h"
#include "config/CemuConfig.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "JNIUtils.h"

namespace NativeGraphicPacks
{
	std::unordered_map<sint64, GraphicPackPtr> s_graphicPacks;

	void SaveGraphicPackStateToConfig(const GraphicPackPtr& graphicPack)
	{
		auto& data = GetConfig();
		auto filename = _utf8ToPath(graphicPack->GetNormalizedPathString());
		if (data.graphic_pack_entries.contains(filename))
			data.graphic_pack_entries.erase(filename);
		if (graphicPack->IsEnabled())
		{
			data.graphic_pack_entries.try_emplace(filename);
			auto& it = data.graphic_pack_entries[filename];
			// otherwise store all selected presets
			for (const auto& preset : graphicPack->GetActivePresets())
				it.try_emplace(preset->category, preset->name);
		}
		else if (graphicPack->IsDefaultEnabled())
		{
			// save that its disabled
			data.graphic_pack_entries.try_emplace(filename);
			auto& it = data.graphic_pack_entries[filename];
			it.try_emplace("_disabled", "false");
		}
	}

	jobjectArray GetGraphicPresets(JNIEnv* env, const GraphicPackPtr& graphicPack, sint64 id)
	{
		using namespace std::ranges::views;
		auto graphicPackPresetClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeGraphicPacks$GraphicPackPreset");
		auto graphicPackPresetCtorId = env->GetMethodID(graphicPackPresetClass, "<init>", "(JLjava/lang/String;[Ljava/lang/String;Ljava/lang/String;)V");

		std::vector<std::string> order;
		using CategorizedPresets = std::unordered_map<std::string, std::vector<GraphicPack2::PresetPtr>>;
		CategorizedPresets presets = graphicPack->GetCategorizedPresets(order);

		struct PresetData
		{
			const std::string& category;
			const std::vector<GraphicPack2::PresetPtr>& presets;
		};

		auto presetsData = order |
						   transform([&](const auto& category) { return PresetData{category, presets.at(category)}; }) |
						   filter([](const PresetData& presetData) {
							   return std::ranges::any_of(presetData.presets, [](const auto& p) { return p->visible; });
						   });

		return JNIUtils::CreateObjectArray(
			env,
			graphicPackPresetClass,
			presetsData,
			[&](const auto& presetData) -> jobject {
				jstring categoryJStr = presetData.category.empty() ? nullptr : JNIUtils::ToJString(env, presetData.category);
				std::vector<std::string> presetSelections;
				std::optional<std::string> activePreset;
				for (auto& pentry : presetData.presets)
				{
					if (!pentry->visible)
						continue;

					presetSelections.push_back(pentry->name);

					if (pentry->active)
						activePreset = pentry->name;
				}

				jstring activePresetJstr = nullptr;

				if (activePreset.has_value())
					activePresetJstr = JNIUtils::ToJString(env, activePreset.value());
				else if (!presetSelections.empty())
					activePresetJstr = JNIUtils::ToJString(env, presetSelections.front());

				return env->NewObject(graphicPackPresetClass, graphicPackPresetCtorId, id, categoryJStr, JNIUtils::CreateStringObjectArray(env, presetSelections), activePresetJstr);
			});
	}
} // namespace NativeGraphicPacks

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_refreshGraphicPacks([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	if (CafeSystem::IsTitleRunning())
	{
		return;
	}

	GraphicPack2::ClearGraphicPacks();
	GraphicPack2::LoadAll();
	NativeGraphicPacks::s_graphicPacks.clear();

	auto graphicPacks = GraphicPack2::GetGraphicPacks();

	for (auto&& graphicPack : graphicPacks)
	{
		NativeGraphicPacks::s_graphicPacks[reinterpret_cast<sint64>(graphicPack.get())] = graphicPack;
	}
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_getGraphicPackBasicInfos(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	jclass graphicPackInfoClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeGraphicPacks$GraphicPackBasicInfo");
	jmethodID graphicPackInfoCtorId = env->GetMethodID(graphicPackInfoClass, "<init>", "(JLjava/lang/String;Z[J)V");

	return JNIUtils::CreateObjectArray(
		env,
		graphicPackInfoClass,
		NativeGraphicPacks::s_graphicPacks,
		[&](const auto& graphicPack) {
			jstring virtualPath = JNIUtils::ToJString(env, graphicPack.second->GetVirtualPath());
			jlong id = graphicPack.first;
			jlongArray titleIds = JNIUtils::CreateLongArray(env, graphicPack.second->GetTitleIds());
			return env->NewObject(graphicPackInfoClass, graphicPackInfoCtorId, id, virtualPath, graphicPack.second->IsEnabled(), titleIds);
		});
}

extern "C" [[maybe_unused]] JNIEXPORT jobject JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_getGraphicPack(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id)
{
	jclass graphicPackClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeGraphicPacks$GraphicPack");
	jmethodID graphicPackCtorId = env->GetMethodID(graphicPackClass, "<init>", "(JZLjava/lang/String;Ljava/lang/String;[Linfo/cemu/cemu/nativeinterface/NativeGraphicPacks$GraphicPackPreset;)V");
	const auto& graphicPack = NativeGraphicPacks::s_graphicPacks.at(id);

	jstring graphicPackName = JNIUtils::ToJString(env, graphicPack->GetName());
	jstring graphicPackDescription = JNIUtils::ToJString(env, graphicPack->GetDescription());
	return env->NewObject(
		graphicPackClass,
		graphicPackCtorId,
		id,
		graphicPack->IsEnabled(),
		graphicPackName,
		graphicPackDescription,
		NativeGraphicPacks::GetGraphicPresets(env, graphicPack, id));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_setGraphicPackActive([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id, jboolean active)
{
	const auto& graphicPack = NativeGraphicPacks::s_graphicPacks.at(id);
	graphicPack->SetEnabled(active);
	NativeGraphicPacks::SaveGraphicPackStateToConfig(graphicPack);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_setGraphicPackActivePreset([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id, jstring category, jstring preset)
{
	std::string presetCategory = category == nullptr ? "" : JNIUtils::FromJString(env, category);
	const auto& graphicPack = NativeGraphicPacks::s_graphicPacks.at(id);
	graphicPack->SetActivePreset(presetCategory, JNIUtils::FromJString(env, preset));
	NativeGraphicPacks::SaveGraphicPackStateToConfig(graphicPack);
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeGraphicPacks_getGraphicPackPresets(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id)
{
	return NativeGraphicPacks::GetGraphicPresets(env, NativeGraphicPacks::s_graphicPacks.at(id), id);
}
