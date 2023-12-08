################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../winc3400/common/source/efuse.c \
../winc3400/common/source/nm_common.c 

OBJS += \
./winc3400/common/source/efuse.o \
./winc3400/common/source/nm_common.o 

C_DEPS += \
./winc3400/common/source/efuse.d \
./winc3400/common/source/nm_common.d 


# Each subdirectory must supply rules for building sources it contributes
winc3400/common/source/%.o winc3400/common/source/%.su winc3400/common/source/%.cyclo: ../winc3400/common/source/%.c winc3400/common/source/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F429xx -DCONF_PERIPH -DWIFI_PROVISIONING -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../MP3 -I../Middlewares/Third_Party/FatFs/src -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../winc3400 -I../winc3400/bsp/include -I../winc3400/bus_wrapper/include -I../winc3400/common/include -I../winc3400/config -I../winc3400/driver/include -I../winc3400/programmer -I../winc3400/socket/include -I../winc3400/spi_flash/include -I../winc3400/ble/atmel_ble_api/include -I../winc3400/ble/inc -I../winc3400/ble/utils -I../winc3400/winc3400_ble_api/include -I../winc3400/ble/ble_profiles/wifi_provisioning -I../winc3400/ble/ble_services/ble_mgr -I../winc3400/ble/ble_services/wifi_con -I../winc3400/ble/ble_services/wifi_scan -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-winc3400-2f-common-2f-source

clean-winc3400-2f-common-2f-source:
	-$(RM) ./winc3400/common/source/efuse.cyclo ./winc3400/common/source/efuse.d ./winc3400/common/source/efuse.o ./winc3400/common/source/efuse.su ./winc3400/common/source/nm_common.cyclo ./winc3400/common/source/nm_common.d ./winc3400/common/source/nm_common.o ./winc3400/common/source/nm_common.su

.PHONY: clean-winc3400-2f-common-2f-source

