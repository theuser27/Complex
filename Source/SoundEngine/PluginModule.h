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
	class PluginModule;

	// global state for keeping track of all parameters lists through global module number
	class AllModules
	{
	protected:
		AllModules() = default;
		AllModules(const AllModules &) = default;
		AllModules(AllModules &&) = default;
		AllModules &operator=(const AllModules &other) = default;
		AllModules &operator=(AllModules &&other) = default;

	public:
		// may block and may allocate, if expandThreshold has been reached
		bool addModule(std::shared_ptr<PluginModule> newPointer) noexcept;
		bool deleteModule(u64 moduleId) noexcept;
		std::weak_ptr<Framework::ParameterValue> getModuleParameter(u64 parentModuleId, std::string_view parameter) const noexcept;
		u64 getId(bool isTopLevelModule = false) noexcept
		{
			return (isTopLevelModule) ? globalModuleIdCounter.load(std::memory_order_acquire) :
				globalModuleIdCounter.fetch_add(1, std::memory_order_acq_rel);
		}

		UpdateFlag getUpdateFlag() const noexcept { return updateFlag.load(std::memory_order_acquire); }
		void setUpdateFlag(UpdateFlag newFlag) noexcept { updateFlag.store(newFlag, std::memory_order_release); }

	private:
		void resizeAllParameters() noexcept;

		std::unique_ptr<Framework::VectorMap<u64, std::weak_ptr<PluginModule>>> allModules =
			std::make_unique<Framework::VectorMap<u64, std::weak_ptr<PluginModule>>>(64);

		// used to give out non-repeating ids for all PluginModules
		std::atomic<u64> globalModuleIdCounter = 0;
		// used for checking whether it's ok to update parameters/plugin structure
		std::atomic<UpdateFlag> updateFlag{ UpdateFlag::AfterProcess };

		mutable std::atomic<bool> currentlyInUse = false;

		static constexpr size_t expandAmount = 2;
		static constexpr float expandThreshold = 0.75f;
	};

	class PluginModule
	{
	public:
		PluginModule() = delete;
		PluginModule(const PluginModule &) = delete;
		PluginModule(PluginModule &&) = delete;

		PluginModule(AllModules *globalModulesState_, u64 parentModuleId, std::string_view moduleType) noexcept;
		PluginModule(const PluginModule &other, u64 parentModuleId) noexcept;
		PluginModule &operator=(const PluginModule &other) noexcept;
		PluginModule(PluginModule &&other, u64 parentModuleId) noexcept;
		PluginModule &operator=(PluginModule &&other) noexcept;

		virtual ~PluginModule() noexcept { globalModulesState_->deleteModule(moduleId_); }

		virtual void initialise() noexcept
		{
			for (size_t i = 0; i < moduleParameters_.data.size(); i++)
				moduleParameters_.data[i].second->initialise();
		}

		virtual std::shared_ptr<PluginModule> createCopy(u64 parentModuleId) const noexcept = 0;

		void clearSubModules() noexcept { subModules_.clear(); }
		bool checkUpdateFlag() noexcept
		{
			if (auto flag = globalModulesState_->getUpdateFlag();
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
		void setParentModuleId(u64 newParentModuleId) noexcept { parentModuleId_ = newParentModuleId; }

		// how many threads are currently using this object
		i8 getNumCurrentUsers() noexcept { return currentlyUsing_.load(std::memory_order_acquire); }
		// flags this object as not to be used and exit all parameter links
		void softDelete() noexcept { utils::spinAndLock(currentlyUsing_, 0, -1); }
		// opposite of softDelete
		void reuse() noexcept { currentlyUsing_.store(0, std::memory_order_release); }


		// creates and registers a submodule to the global state
		// use this when creating submodules
		template<typename T, typename ... Args>
		std::shared_ptr<PluginModule> createSubModule(Args&& ... args)
		{
			if constexpr (sizeof...(Args) == 0)
			{
				std::shared_ptr<PluginModule> newSubModule(new T(globalModulesState_, moduleId_));
				globalModulesState_->addModule(newSubModule);
				return newSubModule;
			}
			else
			{
				std::shared_ptr<PluginModule> newSubModule(new T(globalModulesState_, moduleId_, std::forward<Args>(args)...));
				globalModulesState_->addModule(newSubModule);
				return newSubModule;
			}
		}

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
		u64 parentModuleId_{};

		AllModules *const globalModulesState_ = nullptr;
		const std::string_view moduleType_{};
		const u64 moduleId_{};

		friend class AllModules;
	};
}