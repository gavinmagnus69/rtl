#pragma once

#include <variant>

#include "GetDeviceCapabilitiesRequestDto.hpp"
#include "GetDeviceStateRequestDto.hpp"
#include "ListDevicesRequestDto.hpp"
#include "SetDeviceParameterRequestDto.hpp"
#include "StartConnectRequestDto.hpp"
#include "StartDisconnectRequestDto.hpp"
#include "StartStreamRequestDto.hpp"
#include "StopStreamRequestDto.hpp"
#include "SubscribeTelemetryRequestDto.hpp"
#include "UnsubscribeTelemetryRequestDto.hpp"

namespace ws::dto {

using WsRequestDtoVariant =
    std::variant<ListDevicesRequestDto, GetDeviceStateRequestDto,
                 GetDeviceCapabilitiesRequestDto, StartConnectRequestDto,
                 StartDisconnectRequestDto, StartStreamRequestDto,
                 StopStreamRequestDto, SubscribeTelemetryRequestDto,
                 UnsubscribeTelemetryRequestDto, SetDeviceParameterRequestDto>;

} // namespace ws::dto
