/*
	==============================================================================

		PluginModule.h
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "./Framework/common.h"
#include "./Framework/vector_map.h"
#include "./Framework/parameters.h"
#include "./Framework/parameter_value.h"

namespace Generation
{
	class PluginModule
	{
	public:
		PluginModule() = delete;
		PluginModule(const PluginModule &) = delete;
		PluginModule(PluginModule &&) = delete;

		PluginModule(u64 parentModuleId, std::string_view moduleType) noexcept;
		PluginModule(const PluginModule &other, u64 parentModuleId) noexcept;
		PluginModule &operator=(const PluginModule &other) noexcept;
		PluginModule(PluginModule &&other, u64 parentModuleId) noexcept;
		PluginModule &operator=(PluginModule &&other) noexcept;

		virtual ~PluginModule() noexcept { AllModules::deleteModule(moduleId_); }

		virtual void initialise() noexcept
		{
			for (size_t i = 0; i < moduleParameters_.data.size(); i++)
				moduleParameters_.data[i].second->initialise();
		}

		void clearSubModules() noexcept { subModules_.clear(); }
		static bool checkUpdateFlag() noexcept
		{
			if (auto flag = RuntimeInfo::updateFlag.load(std::memory_order_acquire);
				!(flag == UpdateFlag::AfterProcess || flag == UpdateFlag::BeforeProcess))
				return false;
			return true;
		}

		// the following functions are to be called outside of processing time
		virtual bool insertSubModule([[maybe_unused]] u32 index, [[maybe_unused]] std::string_view moduleType) noexcept
		{ return checkUpdateFlag(); }
		virtual bool deleteSubModule([[maybe_unused]] u32 index) noexcept
		{ return checkUpdateFlag(); }
		virtual bool copySubModule([[maybe_unused]] const std::shared_ptr<PluginModule> &newSubModule, [[maybe_unused]] u32 index) noexcept
		{ return checkUpdateFlag(); }
		virtual bool moveSubModule([[maybe_unused]] std::shared_ptr<PluginModule> newSubModule, [[maybe_unused]] u32 index) noexcept
		{ return checkUpdateFlag(); }
		
		void updateParameters(UpdateFlag flag, bool updateSubModuleParameters = true);

		void randomiseParameters();
		void setAllParametersRandomisation(bool toRandomise = true);
		void setParameterRandomisation(std::string_view name, bool toRandomise = true);

		std::string_view getModuleType() const noexcept { return moduleType_; }
		u64 getModuleId() const noexcept { return moduleId_; }
		u64 getParentModuleId() const noexcept { return parentModuleId_; }

		// how many threads are currently using this object
		i8 getNumCurrentUsers() noexcept { return currentlyUsing_.load(std::memory_order_acquire); }
		// flags this object as not to be used and exit all parameter links
		void softDelete() noexcept { utils::spinAndLock(currentlyUsing_, 0, -1); }
		// opposite of softDelete
		void reuse() noexcept { currentlyUsing_.store(0, std::memory_order_release); }

	protected:
		void createModuleParameters(const Framework::ParameterDetails *details, size_t size) noexcept;
		void addSubModulesToList() noexcept;

		// number of threads currently using this module
		// if it's < 0, then this module is deleted/is not to be used
		std::atomic<i8> currentlyUsing_{};

		// data contextual to every individual module
		std::vector<std::shared_ptr<PluginModule>> subModules_{};
		Framework::VectorMap<std::string_view, std::shared_ptr<Framework::ParameterValue>> moduleParameters_{};

		// data contextual to the base PluginModule
		const std::string_view moduleType_{};
		const u64 moduleId_{};
		const u64 parentModuleId_{};

		// used to give out non-repeating ids for all PluginModules, zero-initialised at loading time
		// TODO: see if this even needs to be thread-safe
		// TODO: how are you going to save this in presets?
		//       maybe, go through all modules, find the largest index and continue from there?
		
		// TODO: rework the module ID system so that it doesn't utilise globals, because all instances of the plugin see them!!!!!!!
		inline static std::atomic<u64> globalModuleIdCounter = 0;
	public:
		// global state for keeping track of all parameters lists through global module number
		struct AllModules
		{
			// may block and may allocate, if expandThreshold has been reached
			static bool addModule(std::shared_ptr<PluginModule> newPointer) noexcept;
			static bool deleteModule(u64 moduleId) noexcept;
			static std::weak_ptr<Framework::ParameterValue> getModuleParameter(u64 parentModuleId, std::string_view parameter) noexcept;

		private:
			static void resizeAllParameters() noexcept;
			inline static auto allModules = std::make_unique<Framework::VectorMap<u64,
				std::weak_ptr<PluginModule>>>(64);
			inline static std::atomic<bool> currentlyInUse = false;

			static constexpr size_t expandAmount = 2;
			static constexpr float expandThreshold = 0.75f;
		};
	};
}