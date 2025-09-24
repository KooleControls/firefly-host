#pragma once
#include <string>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Task
{
	std::string name = "New task";
	portBASE_TYPE priority = 0;
	portSHORT stackDepth = configMINIMAL_STACK_SIZE;
	TaskHandle_t taskHandle = nullptr;
	std::function<void()> callback;

	static void TaskFunction(void* parm)
	{
		Task* t = static_cast<Task*>(parm);
		if (t && t->callback) {
			t->callback();
		}
		vTaskDelete(nullptr);
	}

	void DeleteTaskIfExists()
	{
		if (taskHandle != nullptr) {
			vTaskDelete(taskHandle);
			taskHandle = nullptr;
		}
	}

public:

	Task() = default;

	~Task()
	{
		DeleteTaskIfExists();
	}

	bool Init(const std::string& name, portBASE_TYPE priority, portSHORT stackDepth)
	{
		this->name = name;
		this->priority = priority;
		this->stackDepth = (stackDepth < configMINIMAL_STACK_SIZE) ? configMINIMAL_STACK_SIZE : stackDepth;
		return true;
	}

	bool SetHandler(const std::function<void()>& callback)
	{
		this->callback = callback;
		return true;
	}

	bool Run(BaseType_t core = tskNO_AFFINITY)
	{
		DeleteTaskIfExists();
		if (core == tskNO_AFFINITY) {
			xTaskCreate(&TaskFunction, name.c_str(), stackDepth, this, priority, &taskHandle);
		} else {
			xTaskCreatePinnedToCore(&TaskFunction, name.c_str(), stackDepth, this, priority, &taskHandle, core);
		}
		return taskHandle != nullptr;
	}

#ifdef configUSE_TASK_NOTIFICATIONS

	bool NotifyWait(uint32_t* pulNotificationValue, TickType_t timeout = portMAX_DELAY)
	{
		return xTaskNotifyWait(0x0000, 0xFFFF, pulNotificationValue, timeout) == pdPASS;
	}

	bool Notify(uint32_t bits)
	{
		return xTaskNotify(taskHandle, bits, eSetBits) == pdPASS;
	}

	bool NotifyFromISR(uint32_t bits)
	{
		return xTaskNotifyFromISR(taskHandle, bits, eSetBits, nullptr) == pdPASS;
	}

	bool NotifyFromISR(uint32_t bits, BaseType_t* pxHigherPriorityTaskWoken)
	{
		return xTaskNotifyFromISR(taskHandle, bits, eSetBits, pxHigherPriorityTaskWoken) == pdPASS;
	}

#endif // configUSE_TASK_NOTIFICATIONS

	static int GetCurrentCoreID()
	{
		return xPortGetCoreID();
	}
};
