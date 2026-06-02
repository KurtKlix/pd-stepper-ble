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
    long             Timestamp
);
