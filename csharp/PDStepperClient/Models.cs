using System.Text.Json.Serialization;

namespace PDStepper;

public record DeviceInfo(
    [property: JsonPropertyName("address")] string Address,
    [property: JsonPropertyName("name")]    string Name,
    [property: JsonPropertyName("rssi")]    int    Rssi
);

public record StatusResponse(
    [property: JsonPropertyName("connected")]    bool                    Connected,
    [property: JsonPropertyName("state")]        string                  State,
    [property: JsonPropertyName("address")]      string?                 Address,
    [property: JsonPropertyName("last_status")]  Dictionary<string,object?> LastStatus
);

public record ConfigureRequest
{
    [JsonPropertyName("current_ma")]        public int? CurrentMa        { get; init; }
    [JsonPropertyName("microsteps")]        public int? Microsteps       { get; init; }
    [JsonPropertyName("speed_sps")]         public int? SpeedSps         { get; init; }
    [JsonPropertyName("closed_loop_type")]  public int? ClosedLoopType   { get; init; }
    [JsonPropertyName("mapping_direction")] public int? MappingDirection { get; init; }
    [JsonPropertyName("home_direction")]    public int? HomeDirection    { get; init; }  // 1 or -1: direction toward the endstop
}

/// <summary>Homing mode + sensorless (StallGuard) tuning — POST /api/homing_config. All fields optional.</summary>
public record HomingConfigRequest
{
    [JsonPropertyName("homing_mode")]           public string? HomingMode          { get; init; }  // "endstop" or "sensorless"
    [JsonPropertyName("sensorless_current_ma")] public int?    SensorlessCurrentMa { get; init; }
    [JsonPropertyName("sgthrs")]                public int?    Sgthrs              { get; init; }  // StallGuard threshold 0-255 (higher = more sensitive)
    [JsonPropertyName("sensorless_speed_sps")]  public int?    SensorlessSpeedSps  { get; init; }
}

/// <summary>Event types received from the WebSocket stream.</summary>
public enum StepperEventType
{
    Unknown,
    Status,
    PositionReached,
    Stall,
    Endstop,
    HomeComplete,
    Ack,
    Error,
}

public record StepperEvent(
    StepperEventType Type,
    double?          PosDeg,
    double?          TargetDeg,
    double?          VelDps,
    double?          VoltageV,
    double?          PosMm,       // linear position in mm (requires belt config)
    double?          TargetMm,
    double?          VelMmS,
    string?          Message,
    string?          Cmd,
    bool?            Ok,
    long             Timestamp,
    // Extended status fields (status events): drive mode + endstop / StallGuard debug telemetry.
    string?          Mode       = null,   // idle / position / velocity / homing / sensorless_homing
    int?             EndstopPin = null,   // raw GPIO state: 1 = idle, 0 = triggered
    long?            EndstopIsr = null,   // cumulative endstop interrupt count
    int?             SgResult   = null    // StallGuard reading 0-510 during sensorless_homing, else -1
);
