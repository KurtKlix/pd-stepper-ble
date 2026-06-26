using System.Diagnostics;
using System.Net.Http.Json;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace PDStepper;

/// <summary>
/// HTTP + WebSocket client for the PD-Stepper BLE API service.
/// The API service (PD_Stepper_API.exe) must be running on the host before use.
/// </summary>
public sealed class PDStepperClient : IDisposable
{
    private readonly HttpClient          _http;
    private readonly string              _baseUrl;
    private          ClientWebSocket?    _ws;
    private          CancellationTokenSource? _wsCts;
    private static readonly JsonSerializerOptions _json = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    // ── Events ────────────────────────────────────────────────────────────────

    public event Action<StepperEvent>? OnStatusUpdate;
    public event Action<StepperEvent>? OnPositionReached;
    public event Action<StepperEvent>? OnStall;
    public event Action<StepperEvent>? OnEndstop;
    public event Action<StepperEvent>? OnHomeComplete;
    public event Action<StepperEvent>? OnAck;
    public event Action<StepperEvent>? OnError;
    /// <summary>Fires for every event regardless of type.</summary>
    public event Action<StepperEvent>? OnAnyEvent;

    // ── Constructor ───────────────────────────────────────────────────────────

    /// <param name="baseUrl">Base URL of the API service, e.g. http://localhost:8000</param>
    public PDStepperClient(string baseUrl = "http://localhost:8000")
    {
        _baseUrl = baseUrl.TrimEnd('/');
        _http = new HttpClient { BaseAddress = new Uri(_baseUrl + "/") };
    }

    // ── Discovery & connection ────────────────────────────────────────────────

    public async Task<List<DeviceInfo>> ScanAsync(double durationSeconds = 5.0, CancellationToken ct = default)
    {
        var result = await _http.GetFromJsonAsync<List<DeviceInfo>>(
            $"api/scan?duration={durationSeconds}", _json, ct);
        return result ?? [];
    }

    public async Task ConnectAsync(string address, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/connect", new { address }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task DisconnectAsync(CancellationToken ct = default)
    {
        var res = await _http.PostAsync("api/disconnect", null, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task<StatusResponse?> GetStatusAsync(CancellationToken ct = default)
        => await _http.GetFromJsonAsync<StatusResponse>("api/status", _json, ct);

    public async Task<JsonElement> GetConfigAsync(CancellationToken ct = default)
    {
        using var res = await _http.GetAsync("api/config", ct);
        res.EnsureSuccessStatusCode();
        var body = await res.Content.ReadAsStringAsync(ct);
        return JsonSerializer.Deserialize<JsonElement>(body);
    }

    // ── Motion ────────────────────────────────────────────────────────────────

    public async Task MoveAbsoluteAsync(double degrees, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/move/absolute", new { degrees }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task MoveRelativeAsync(double degrees, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/move/relative", new { degrees }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    /// <param name="dps">Degrees per second. Negative = reverse. 0 = stop.</param>
    public async Task SetVelocityAsync(double dps, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/velocity", new { dps }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task StopAsync(CancellationToken ct = default)
    {
        var res = await _http.PostAsync("api/stop", null, ct);
        res.EnsureSuccessStatusCode();
    }

    // ── mm-based motion (requires belt config set via ConfigureBeltAsync) ─────

    public async Task MoveAbsoluteMmAsync(double mm, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/move/absolute_mm", new { mm }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task MoveRelativeMmAsync(double mm, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/move/relative_mm", new { mm }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    /// <param name="mmPerS">mm per second. Negative = reverse. 0 = stop.</param>
    public async Task SetVelocityMmAsync(double mmPerS, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/velocity_mm", new { mm_per_s = mmPerS }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task ConfigureBeltAsync(double mmPerRev, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/belt", new { mm_per_rev = mmPerRev }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task<(double MmPerRev, double MmPerDeg)> GetBeltConfigAsync(CancellationToken ct = default)
    {
        using var res = await _http.GetAsync("api/belt", ct);
        res.EnsureSuccessStatusCode();
        var json = JsonSerializer.Deserialize<JsonElement>(await res.Content.ReadAsStringAsync(ct));
        return (json.GetDoubleOrNull("mm_per_rev") ?? 40.0,
                json.GetDoubleOrNull("mm_per_deg") ?? 40.0 / 360.0);
    }

    public async Task<bool> MoveAndWaitMmAsync(double mm, TimeSpan timeout, CancellationToken ct = default)
    {
        var tcs = new TaskCompletionSource<bool>();
        void OnReached(StepperEvent _) => tcs.TrySetResult(true);
        void OnStalled(StepperEvent _) => tcs.TrySetResult(false);
        OnPositionReached += OnReached;
        OnStall           += OnStalled;
        try
        {
            await MoveAbsoluteMmAsync(mm, ct);
            var timeoutTask = Task.Delay(timeout, ct);
            var winner = await Task.WhenAny(tcs.Task, timeoutTask);
            return winner == tcs.Task && tcs.Task.Result;
        }
        finally
        {
            OnPositionReached -= OnReached;
            OnStall           -= OnStalled;
        }
    }

    public async Task HomeAsync(CancellationToken ct = default)
    {
        var res = await _http.PostAsync("api/home", null, ct);
        res.EnsureSuccessStatusCode();
    }

    // ── Configuration ─────────────────────────────────────────────────────────

    public async Task ConfigureAsync(ConfigureRequest config, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/configure", config, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    public async Task SetEnabledAsync(bool enabled, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/enable", new { enabled }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    /// <summary>Set the endstop switch polarity (NO/NC) — applied immediately, no reflash. POST /api/endstop_mode.</summary>
    public async Task SetEndstopModeAsync(bool normallyClosed, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/endstop_mode", new { normally_closed = normallyClosed }, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    /// <summary>Set homing mode (endstop/sensorless) and tune sensorless StallGuard params. POST /api/homing_config.</summary>
    public async Task ConfigureHomingAsync(HomingConfigRequest config, CancellationToken ct = default)
    {
        var res = await _http.PostAsJsonAsync("api/homing_config", config, _json, ct);
        res.EnsureSuccessStatusCode();
    }

    // ── WebSocket event stream ────────────────────────────────────────────────

    /// <summary>
    /// Start receiving real-time events from the motor. Events are raised on
    /// a background thread — marshal to UI thread as needed.
    /// </summary>
    public async Task StartEventStreamAsync(CancellationToken ct = default)
    {
        _wsCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _ws    = new ClientWebSocket();

        var wsUri = new Uri(_baseUrl.Replace("http://", "ws://")
                                    .Replace("https://", "wss://") + "/ws");
        await _ws.ConnectAsync(wsUri, _wsCts.Token);
        _ = Task.Run(() => WsReceiveLoop(_wsCts.Token), _wsCts.Token);
    }

    public async Task StopEventStreamAsync()
    {
        _wsCts?.Cancel();
        if (_ws?.State == WebSocketState.Open)
            await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
    }

    private async Task WsReceiveLoop(CancellationToken ct)
    {
        var buf = new byte[4096];
        try
        {
            while (_ws!.State == WebSocketState.Open && !ct.IsCancellationRequested)
            {
                var result = await _ws.ReceiveAsync(buf, ct);
                if (result.MessageType == WebSocketMessageType.Close) break;

                var json    = JsonSerializer.Deserialize<JsonElement>(buf.AsSpan(0, result.Count));
                var evt     = ParseEvent(json);
                DispatchEvent(evt);
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            DispatchEvent(new StepperEvent(
                StepperEventType.Error, null, null, null, null, null, null, null,
                $"WebSocket error: {ex.Message}", null, null, 0));
        }
    }

    private static StepperEvent ParseEvent(JsonElement json)
    {
        var typeStr   = json.GetStringOrNull("type") ?? "";
        var type      = typeStr switch
        {
            "status"           => StepperEventType.Status,
            "position_reached" => StepperEventType.PositionReached,
            "stall"            => StepperEventType.Stall,
            "endstop"          => StepperEventType.Endstop,
            "home_complete"    => StepperEventType.HomeComplete,
            "ack"              => StepperEventType.Ack,
            "error"            => StepperEventType.Error,
            _                  => StepperEventType.Unknown,
        };

        return new StepperEvent(
            Type:       type,
            PosDeg:     json.GetDoubleOrNull("pos_deg"),
            TargetDeg:  json.GetDoubleOrNull("target_deg"),
            VelDps:     json.GetDoubleOrNull("vel_dps"),
            VoltageV:   json.GetDoubleOrNull("voltage_v"),
            PosMm:      json.GetDoubleOrNull("pos_mm"),
            TargetMm:   json.GetDoubleOrNull("target_mm"),
            VelMmS:     json.GetDoubleOrNull("vel_mm_s"),
            Message:    json.GetStringOrNull("msg"),
            Cmd:        json.GetStringOrNull("cmd"),
            Ok:         json.GetBoolOrNull("ok"),
            Timestamp:  json.GetLongOrNull("ts") ?? 0,
            Mode:       json.GetStringOrNull("mode"),
            EndstopPin: (int?)json.GetLongOrNull("endstop_pin"),
            EndstopIsr: json.GetLongOrNull("endstop_isr"),
            SgResult:   (int?)json.GetLongOrNull("sg_result")
        );
    }

    private void DispatchEvent(StepperEvent evt)
    {
        OnAnyEvent?.Invoke(evt);
        switch (evt.Type)
        {
            case StepperEventType.Status:           OnStatusUpdate?.Invoke(evt);    break;
            case StepperEventType.PositionReached:  OnPositionReached?.Invoke(evt); break;
            case StepperEventType.Stall:            OnStall?.Invoke(evt);           break;
            case StepperEventType.Endstop:          OnEndstop?.Invoke(evt);         break;
            case StepperEventType.HomeComplete:     OnHomeComplete?.Invoke(evt);    break;
            case StepperEventType.Ack:              OnAck?.Invoke(evt);             break;
            case StepperEventType.Error:            OnError?.Invoke(evt);           break;
        }
    }

    // ── Convenience: wait for position reached ────────────────────────────────

    /// <summary>
    /// Move to an absolute position and await completion or stall.
    /// Returns true on position_reached, false on stall/timeout.
    /// </summary>
    public async Task<bool> MoveAndWaitAsync(double degrees, TimeSpan timeout, CancellationToken ct = default)
    {
        var tcs = new TaskCompletionSource<bool>();

        void OnReached(StepperEvent _) => tcs.TrySetResult(true);
        void OnStalled(StepperEvent _) => tcs.TrySetResult(false);

        OnPositionReached += OnReached;
        OnStall           += OnStalled;

        try
        {
            await MoveAbsoluteAsync(degrees, ct);
            var timeoutTask = Task.Delay(timeout, ct);
            var winner = await Task.WhenAny(tcs.Task, timeoutTask);
            return winner == tcs.Task && tcs.Task.Result;
        }
        finally
        {
            OnPositionReached -= OnReached;
            OnStall           -= OnStalled;
        }
    }

    // ── Service launcher ──────────────────────────────────────────────────────

    /// <summary>
    /// Ensure the API service exe is running. If a Windows service named
    /// "PDStepperAPI" exists and is running, does nothing. Otherwise launches
    /// the exe directly from the given path.
    /// </summary>
    [System.Runtime.Versioning.SupportedOSPlatform("windows")]
    public static void EnsureApiRunning(string exePath = "PD_Stepper_API.exe")
    {
        // Try Windows service first
        try
        {
            using var sc = new System.ServiceProcess.ServiceController("PDStepperAPI");
            if (sc.Status == System.ServiceProcess.ServiceControllerStatus.Running)
                return;
        }
        catch { /* service not installed */ }

        // Fall back to direct process launch
        if (!System.IO.File.Exists(exePath)) return;

        Process.Start(new ProcessStartInfo
        {
            FileName        = exePath,
            CreateNoWindow  = true,
            UseShellExecute = false,
        });

        // Give the server a moment to start
        Thread.Sleep(1500);
    }

    public void Dispose()
    {
        _wsCts?.Cancel();
        _ws?.Dispose();
        _wsCts?.Dispose();
        _http.Dispose();
    }
}

// ── JsonElement extension helpers ─────────────────────────────────────────────

internal static class JsonElementExtensions
{
    public static string? GetStringOrNull(this JsonElement el, string prop)
        => el.TryGetProperty(prop, out var v) && v.ValueKind == JsonValueKind.String
           ? v.GetString() : null;

    public static double? GetDoubleOrNull(this JsonElement el, string prop)
        => el.TryGetProperty(prop, out var v) && v.ValueKind == JsonValueKind.Number
           ? v.GetDouble() : null;

    public static bool? GetBoolOrNull(this JsonElement el, string prop)
        => el.TryGetProperty(prop, out var v) &&
           (v.ValueKind == JsonValueKind.True || v.ValueKind == JsonValueKind.False)
           ? v.GetBoolean() : null;

    public static long? GetLongOrNull(this JsonElement el, string prop)
        => el.TryGetProperty(prop, out var v) && v.ValueKind == JsonValueKind.Number
           ? v.GetInt64() : null;
}
