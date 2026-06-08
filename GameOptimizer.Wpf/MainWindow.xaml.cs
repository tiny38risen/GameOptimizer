using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace GameOptimizer.Wpf;

public partial class MainWindow : Window
{
    private readonly List<string> hiddenLogLines = new();

    private Process? runningProcess;
    private int warningCount;
    private int blockerCount;
    private int logSampleCount;
    private double cpuScore = 55;
    private double networkScore = 70;
    private double threadScore = 72;
    private double safetyScore = 92;

    private sealed class ProcessListItem
    {
        public required string ExeName { get; init; }
        public required int ProcessId { get; init; }
        public string WindowTitle { get; init; } = string.Empty;

        public override string ToString()
        {
            return string.IsNullOrWhiteSpace(WindowTitle)
                ? $"{ExeName}  (실행번호 {ProcessId})"
                : $"{ExeName}  (실행번호 {ProcessId}) - {WindowTitle}";
        }
    }

    private sealed class TargetSelection
    {
        public required string ExeName { get; init; }
        public int? ProcessId { get; init; }
    }

    public MainWindow()
    {
        InitializeComponent();
        BackgroundFilterText.Text = FindDefaultBackgroundFilter();
        RefreshProcessList();
        UpdateEnginePathLabel();
        UpdateSummaryState("대기", Brushes.SlateGray, "최적화 대기 중");
        UpdateModeDescription();
        UpdateRuntimeLimitState();
        UpdateControlState(false);
        ResetVisualMetrics();
        SwitchView(DashboardView, "시스템 상태 요약", "현재 PC 상태 실시간 확인 중", BtnDashboard);
    }

    private void BtnDashboard_Click(object sender, RoutedEventArgs e)
    {
        SwitchView(DashboardView, "시스템 상태 요약", "현재 PC 상태 실시간 확인 중", BtnDashboard);
    }

    private void BtnMonitor_Click(object sender, RoutedEventArgs e)
    {
        SwitchView(MonitorView, "성능 모니터", "점검 기록 기반 효율 확인 중", BtnMonitor);
    }

    private void BtnSettings_Click(object sender, RoutedEventArgs e)
    {
        SwitchView(SettingsView, "설정 및 진단", "권장값으로 시작한 뒤 필요한 설정만 조정하세요", BtnSettings);
    }

    private async void BtnOptimize_Click(object sender, RoutedEventArgs e)
    {
        await ToggleEngineAsync();
    }

    private void RefreshButton_Click(object sender, RoutedEventArgs e)
    {
        RefreshProcessList();
    }

    private void ModeRadio_Checked(object sender, RoutedEventArgs e)
    {
        UpdateModeDescription();
        if (ApplyRadio.IsChecked == true && runningProcess is null)
        {
            UpdateSummaryState("주의 필요", Brushes.DarkGoldenrod, "강력 적용 모드 대기 중");
        }
    }

    private void RuntimeLimitCheck_Changed(object sender, RoutedEventArgs e)
    {
        UpdateRuntimeLimitState();
    }

    private void BrowseFilterButton_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.OpenFileDialog
        {
            Title = "예외 프로그램 목록 파일 선택",
            Filter = "텍스트 파일 (*.txt)|*.txt|모든 파일 (*.*)|*.*",
            FileName = Path.GetFileName(BackgroundFilterText.Text),
        };
        if (dialog.ShowDialog(this) == true)
        {
            BackgroundFilterText.Text = dialog.FileName;
            BackgroundFilterCheck.IsChecked = true;
        }
    }

    private void EvidenceFolderButton_Click(object sender, RoutedEventArgs e)
    {
        OpenEvidenceFolder();
    }

    private void LatestReportButton_Click(object sender, RoutedEventArgs e)
    {
        OpenLatestReport();
    }

    private void SwitchView(UIElement view, string headerTitle, string headerSubtitle, Button activeButton)
    {
        DashboardView.Visibility = Visibility.Collapsed;
        MonitorView.Visibility = Visibility.Collapsed;
        SettingsView.Visibility = Visibility.Collapsed;
        view.Visibility = Visibility.Visible;

        TxtHeaderTitle.Text = headerTitle;
        TxtHeaderSub.Text = headerSubtitle;

        ResetNavButton(BtnDashboard);
        ResetNavButton(BtnMonitor);
        ResetNavButton(BtnSettings);
        SetActiveNavButton(activeButton);
    }

    private static void ResetNavButton(Button button)
    {
        button.Background = Brushes.Transparent;
        button.Foreground = new SolidColorBrush(Color.FromRgb(100, 116, 139));
        button.FontWeight = FontWeights.Normal;
    }

    private static void SetActiveNavButton(Button button)
    {
        button.Background = new SolidColorBrush(Color.FromRgb(239, 246, 255));
        button.Foreground = new SolidColorBrush(Color.FromRgb(37, 99, 235));
        button.FontWeight = FontWeights.Bold;
    }

    private void RefreshProcessList()
    {
        var previous = GetSelectedTargetName();
        TargetCombo.Items.Clear();

        var items = Process.GetProcesses()
            .Select(CreateProcessListItem)
            .Where(item => item is not null)
            .Select(item => item!)
            .OrderByDescending(IsLikelyMabinogi)
            .ThenBy(item => item.ExeName, StringComparer.OrdinalIgnoreCase)
            .ThenBy(item => item.ProcessId)
            .ToList();

        foreach (var item in items)
        {
            TargetCombo.Items.Add(item);
        }

        var previousMatch = items.FindIndex(item => string.Equals(item.ExeName, previous, StringComparison.OrdinalIgnoreCase));
        var mabinogiMatch = items.FindIndex(IsLikelyMabinogi);
        if (previousMatch >= 0)
        {
            TargetCombo.SelectedIndex = previousMatch;
        }
        else if (mabinogiMatch >= 0)
        {
            TargetCombo.SelectedIndex = mabinogiMatch;
        }
        else
        {
            TargetCombo.SelectedIndex = -1;
            TargetCombo.Text = string.Empty;
        }

        TxtGameState.Text = mabinogiMatch >= 0 ? "마비노기 실행 확인됨" : "게임 실행 대기 중";
    }

    private static ProcessListItem? CreateProcessListItem(Process process)
    {
        try
        {
            var name = process.ProcessName;
            if (string.IsNullOrWhiteSpace(name))
            {
                return null;
            }

            return new ProcessListItem
            {
                ExeName = name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? name : $"{name}.exe",
                ProcessId = process.Id,
                WindowTitle = process.MainWindowTitle,
            };
        }
        catch
        {
            return null;
        }
    }

    private static bool IsLikelyMabinogi(ProcessListItem item)
    {
        return item.ExeName.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("마비노기", StringComparison.OrdinalIgnoreCase);
    }

    private async Task ToggleEngineAsync()
    {
        if (runningProcess is not null)
        {
            StopEngine();
            return;
        }

        await StartEngineAsync();
    }

    private async Task StartEngineAsync()
    {
        if (runningProcess is not null)
        {
            return;
        }

        var target = GetSelectedTarget();
        if (string.IsNullOrWhiteSpace(target.ExeName))
        {
            MessageBox.Show(this, "목록에서 최적화할 게임을 선택해주세요.", "게임 선택 필요", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        var enginePath = FindEnginePath();
        if (enginePath is null)
        {
            MessageBox.Show(this, "최적화 도구 파일을 찾을 수 없습니다. Release 빌드를 먼저 생성하세요.", "시스템 오류", MessageBoxButton.OK, MessageBoxImage.Error);
            return;
        }

        ClearLog();
        UpdateSummaryState("세팅중", Brushes.RoyalBlue, "최적화 준비 중");
        TxtMonitorSummary.Text = "시스템 환경 구성 중";
        UpdateControlState(true);

        var psi = new ProcessStartInfo
        {
            FileName = enginePath.FullName,
            WorkingDirectory = enginePath.DirectoryName ?? AppContext.BaseDirectory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };

        var arguments = BuildArgumentList(target);
        foreach (var argument in arguments)
        {
            psi.ArgumentList.Add(argument);
        }

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, e) => AppendLogLine(e.Data);
        process.ErrorDataReceived += (_, e) => AppendLogLine(e.Data);
        process.Exited += (_, _) => Dispatcher.Invoke(() =>
        {
            var exitCode = process.ExitCode;
            process.Dispose();
            runningProcess = null;
            UpdateControlState(false);
            UpdateSummaryState("대기", exitCode == 0 ? Brushes.SeaGreen : Brushes.Firebrick, exitCode == 0 ? "최적화 정상 종료됨" : "오류로 인해 중단됨");
            TxtMonitorSummary.Text = exitCode == 0 ? "PC 상태 매우 안정적" : "시스템 불안정으로 보호 중";
            AppendLogLine(exitCode == 0
                ? "[PASS] UI: 최적화 작업이 안전하게 마무리되었습니다."
                : $"[BLOCKER] UI: 시스템 보호를 위해 비정상 종료되었습니다. (코드 {exitCode})");
        });

        try
        {
            runningProcess = process;
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            UpdateSummaryState("적용중", Brushes.SeaGreen, "최적화 적용 중");
            TxtMonitorSummary.Text = "실시간 점검 기록 수집 중";
            AppendLogLine($"[INFO] UI: {enginePath.FullName}");
            AppendLogLine($"[INFO] UI: GameOptimizer.exe {FormatArgumentsForLog(arguments)}");
            await Task.CompletedTask;
        }
        catch (Exception ex)
        {
            runningProcess = null;
            process.Dispose();
            UpdateControlState(false);
            UpdateSummaryState("대기", Brushes.Firebrick, "실행에 실패했습니다.");
            AppendLogLine($"[BLOCKER] UI: 실행 불가 - {ex.Message}");
        }
    }

    private List<string> BuildArgumentList(TargetSelection target)
    {
        var args = new List<string> { target.ExeName };
        if (target.ProcessId is { } processId)
        {
            args.Add("--pid");
            args.Add(processId.ToString(CultureInfo.InvariantCulture));
        }
        if (DryRunRadio.IsChecked == true)
        {
            args.Add("--dry-run");
        }
        if (ApplyRadio.IsChecked == true)
        {
            args.Add("--apply");
        }
        if (ThreadDetailCheck.IsChecked == true)
        {
            args.Add("--thread-detail-log");
        }
        if (BackgroundDetailCheck.IsChecked == true)
        {
            args.Add("--background-detail-log");
        }
        if (LatencyPingCheck.IsChecked == true && !string.IsNullOrWhiteSpace(LatencyPingText.Text))
        {
            args.Add("--latency-ping");
            args.Add(LatencyPingText.Text.Trim());
        }
        if (BackgroundFilterCheck.IsChecked == true && !string.IsNullOrWhiteSpace(BackgroundFilterText.Text))
        {
            args.Add("--background-filter");
            args.Add(BackgroundFilterText.Text.Trim());
        }
        if (RuntimeLimitCheck.IsChecked == true && int.TryParse(RuntimeSecondsText.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var runtimeSeconds))
        {
            args.Add("--max-runtime-seconds");
            args.Add(Math.Clamp(runtimeSeconds, 5, 3600).ToString(CultureInfo.InvariantCulture));
        }
        return args;
    }

    private TargetSelection GetSelectedTarget()
    {
        if (TargetCombo.SelectedItem is ProcessListItem item)
        {
            return new TargetSelection { ExeName = item.ExeName, ProcessId = item.ProcessId };
        }

        return new TargetSelection { ExeName = TargetCombo.Text.Trim() };
    }

    private string GetSelectedTargetName()
    {
        if (TargetCombo.SelectedItem is ProcessListItem item)
        {
            return item.ExeName;
        }

        return TargetCombo.Text.Trim();
    }

    private void StopEngine()
    {
        if (runningProcess is null)
        {
            UpdateSummaryState("대기", Brushes.SeaGreen, "원래 상태 복구 완료");
            AppendLogLine("[INFO] UI: 실행 중인 엔진이 없어 상태만 복구 완료로 표시했습니다.");
            return;
        }

        try
        {
            runningProcess.Kill(entireProcessTree: true);
            UpdateSummaryState("종료중", Brushes.DarkGoldenrod, "최적화 취소 및 복원 중");
            AppendLogLine("[WARN] UI: 사용자가 종료를 요청했습니다.");
        }
        catch (Exception ex)
        {
            AppendLogLine($"[WARN] UI: 종료 실패 - {ex.Message}");
        }
    }

    private void AppendLogLine(string? line)
    {
        if (string.IsNullOrWhiteSpace(line))
        {
            return;
        }

        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.Invoke(() => AppendLogLine(line));
            return;
        }

        UpdateVisualMetrics(line);
        if (line.Contains("[BLOCKER]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[ERROR]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase))
        {
            TxtStatus.Foreground = Brushes.Firebrick;
            TxtOptimizeState.Text = "오류로 인한 강제 중단됨";
        }
        else if (line.Contains("[WARN]", StringComparison.OrdinalIgnoreCase) &&
                 TxtStatus.Foreground != Brushes.Firebrick)
        {
            TxtStatus.Foreground = Brushes.DarkGoldenrod;
        }

        hiddenLogLines.Add(line);
    }

    private void ClearLog()
    {
        hiddenLogLines.Clear();
        ResetVisualMetrics();
    }

    private void ResetVisualMetrics()
    {
        warningCount = 0;
        blockerCount = 0;
        logSampleCount = 0;
        cpuScore = 55;
        networkScore = 70;
        threadScore = 72;
        safetyScore = 92;
        ApplyMetricValues();
        CpuDetail.Text = "상태 확인 대기 중";
        ThreadDetail.Text = "게임 실행 상태 확인 대기";
        NetworkDetail.Text = "통신 상태 확인 대기";
        TxtMonitorSummary.Text = "점검 기록 대기 중";
    }

    private void UpdateVisualMetrics(string line)
    {
        ++logSampleCount;
        var lower = line.ToLowerInvariant();

        if (lower.Contains("warn"))
        {
            ++warningCount;
        }
        if (lower.Contains("blocker") || lower.Contains("error") || lower.Contains("fail"))
        {
            ++blockerCount;
        }

        var cpuMs = TryReadNumberAfter(line, "EMA-sample-cpu:") ?? TryReadNumberAfter(line, "avg-sample-cpu:");
        if (cpuMs is { } cpuValue)
        {
            cpuScore = Smooth(cpuScore, Math.Clamp(100 - (cpuValue * 4), 10, 100));
        }
        else if (lower.Contains("cpu") || lower.Contains("thread"))
        {
            cpuScore = Smooth(cpuScore, blockerCount > 0 ? 35 : warningCount > 0 ? 65 : 82);
        }
        else
        {
            cpuScore = Smooth(cpuScore, runningProcess is null ? 55 : 76);
        }

        var rtt = TryReadNumberAfter(line, "RTT:") ?? TryReadNumberAfter(line, "rtt=");
        if (rtt is { } rttValue)
        {
            networkScore = Smooth(networkScore, Math.Clamp(100 - rttValue, 15, 100));
        }
        else if (lower.Contains("latency") || lower.Contains("network") || lower.Contains("ping"))
        {
            networkScore = Smooth(networkScore, warningCount > 0 ? 62 : 84);
        }
        else
        {
            networkScore = Smooth(networkScore, runningProcess is null ? 70 : 78);
        }

        if (lower.Contains("migration"))
        {
            var migration = TryReadTrailingNumber(line) ?? 1;
            threadScore = Smooth(threadScore, Math.Clamp(92 - (migration * 12), 20, 100));
        }
        else if (lower.Contains("confirmed main tid") || lower.Contains("ema candidate"))
        {
            threadScore = Smooth(threadScore, 88);
        }
        else if (lower.Contains("thread") && lower.Contains("warn"))
        {
            threadScore = Smooth(threadScore, 58);
        }

        safetyScore = blockerCount > 0
            ? Smooth(safetyScore, 25)
            : warningCount > 0
                ? Smooth(safetyScore, Math.Max(55, 92 - warningCount * 8))
                : Smooth(safetyScore, 94);

        ApplyMetricValues();
        CpuDetail.Text = cpuMs.HasValue ? $"처리 속도 {cpuMs.GetValueOrDefault():0.0}ms 수준" : "안정적인 속도 유지 중";
        ThreadDetail.Text = warningCount > 0 ? "약간의 자원 간섭 발견됨" : "완벽하게 집중 처리 중";
        NetworkDetail.Text = rtt.HasValue ? $"지연 시간 {rtt.GetValueOrDefault():0.0}ms" : "매우 부드러운 통신 상태";

        var healthScore = Math.Clamp((cpuScore + networkScore + threadScore + safetyScore) / 4, 0, 100);
        TxtHealthScore.Text = $"{healthScore:0}";
        TxtHeroTitle.Text = healthScore >= 85 ? "PC 성능이 최적의 상태입니다" : healthScore >= 60 ? "최적화가 활발히 진행 중입니다" : "개선이 필요한 상황입니다";
        TxtHeroDesc.Text = blockerCount > 0
            ? "위험 신호가 감지되어 보호를 위해 일부 기능을 제한했습니다. 세부 상태 정보를 확인하세요."
            : warningCount > 0
                ? "작은 지연이나 간섭이 발견되었습니다. 게임 도중 체감될 수 있으니 성능 모니터를 확인해주세요."
                : "매우 안정적이고 부드러운 상태입니다. 이대로 게임을 즐기시면 됩니다.";
        TxtMonitorSummary.Text = $"현재까지 {logSampleCount}번 검사함 · 경고 {warningCount}건 · 위험 {blockerCount}건";
    }

    private void ApplyMetricValues()
    {
        CpuBar.Value = Math.Clamp(cpuScore, 0, 100);
        ThreadBar.Value = Math.Clamp(threadScore, 0, 100);
        NetworkBar.Value = Math.Clamp(networkScore, 0, 100);
        CpuMonitorBar.Value = CpuBar.Value;
        ThreadMonitorBar.Value = ThreadBar.Value;
        NetworkMonitorBar.Value = NetworkBar.Value;
        SafetyMonitorBar.Value = Math.Clamp(safetyScore, 0, 100);
    }

    private void UpdateSummaryState(string state, Brush color, string optimizeText)
    {
        TxtStatus.Text = $"현재 상태 : {state}";
        TxtStatus.Foreground = color;
        TxtOptimizeState.Text = optimizeText;
        TxtRecoveryState.Text = "안전 복구 시스템 작동 중";
    }

    private void UpdateControlState(bool running)
    {
        BtnOptimize.Content = running ? "[ 최적화 종료 ]" : "[ 최적화 시작 ]";
        TargetCombo.IsEnabled = !running;
        RefreshButton.IsEnabled = !running;
        DryRunRadio.IsEnabled = !running;
        SoftApplyRadio.IsEnabled = !running;
        ApplyRadio.IsEnabled = !running;
    }

    private void UpdateModeDescription()
    {
        if (!IsInitialized)
        {
            return;
        }

        if (DryRunRadio.IsChecked == true)
        {
            TxtModeDescription.Text = "현재 모드는 점검 전용입니다. 게임이나 PC 설정은 바꾸지 않고 위험 요소만 찾아냅니다.";
            if (runningProcess is null)
            {
                TxtOptimizeState.Text = "상태 점검 대기 중";
            }
            return;
        }

        if (SoftApplyRadio.IsChecked == true)
        {
            TxtModeDescription.Text = "안전을 최우선으로 하는 최적화 모드입니다. 위험 신호가 감지되면 즉시 원래 상태로 복구됩니다.";
            if (runningProcess is null)
            {
                TxtOptimizeState.Text = "최적화 대기 중";
            }
            return;
        }

        TxtModeDescription.Text = "강력 적용 모드입니다. 성능을 더 적극적으로 끌어올리지만 시스템 실행 설정을 변경할 수 있어 주의가 필요합니다.";
        if (runningProcess is null)
        {
            TxtOptimizeState.Text = "강력 적용 대기 중";
        }
    }

    private void UpdateRuntimeLimitState()
    {
        if (!IsInitialized)
        {
            return;
        }

        RuntimeSecondsText.IsEnabled = RuntimeLimitCheck.IsChecked == true;
        TxtRuntimeDescription.Text = RuntimeLimitCheck.IsChecked == true
            ? "타이머 설정 시간이 지나면 최적화가 자동으로 풀리고 원래 설정으로 복구됩니다."
            : "직접 종료하기 전까지 쾌적한 상태가 계속 유지됩니다.";
    }

    private void UpdateEnginePathLabel()
    {
        var path = FindEnginePath();
        TxtEnginePath.Text = path is null ? "알림: 최적화 도구를 찾지 못했습니다." : $"경로 확인됨: {path.FullName}";
        TxtEnginePath.Foreground = path is null ? Brushes.Firebrick : Brushes.SlateGray;
    }

    private void OpenEvidenceFolder()
    {
        var folder = FindRepoScriptDirectory()?.FullName is { } scriptDir
            ? Path.Combine(scriptDir, "release_gate_logs")
            : Path.Combine(AppContext.BaseDirectory, "release_gate_logs");
        Directory.CreateDirectory(folder);
        Process.Start(new ProcessStartInfo { FileName = folder, UseShellExecute = true });
    }

    private void OpenLatestReport()
    {
        var evidence = FindRepoScriptDirectory()?.GetDirectories("release_gate_logs").FirstOrDefault();
        if (evidence is null || !evidence.Exists)
        {
            MessageBox.Show(this, "아직 저장된 진단 리포트가 없습니다.", "알림", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        var selectedTarget = GetSelectedTargetName();
        var reports = evidence.GetFiles("rc_evidence_report.txt", SearchOption.AllDirectories)
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .ToList();
        var report = string.IsNullOrWhiteSpace(selectedTarget)
            ? reports.FirstOrDefault()
            : reports.FirstOrDefault(file => ReportMatchesTarget(file, selectedTarget));
        if (report is null)
        {
            MessageBox.Show(this, "선택한 게임에 대한 리포트를 찾지 못했습니다.", "알림", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }
        Process.Start(new ProcessStartInfo { FileName = report.FullName, UseShellExecute = true });
    }

    private static FileInfo? FindEnginePath()
    {
        foreach (var directory in WalkUp(new DirectoryInfo(AppContext.BaseDirectory)))
        {
            var candidates = new[]
            {
                Path.Combine(directory.FullName, "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "x64", "Release", "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "GameOptimizer", "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "GameOptimizer", "x64", "Release", "GameOptimizer.exe"),
            };
            foreach (var candidate in candidates)
            {
                if (File.Exists(candidate))
                {
                    return new FileInfo(candidate);
                }
            }
        }
        return null;
    }

    private static DirectoryInfo? FindRepoScriptDirectory()
    {
        foreach (var directory in WalkUp(new DirectoryInfo(AppContext.BaseDirectory)))
        {
            var scriptDir = Path.Combine(directory.FullName, "GameOptimizer");
            if (File.Exists(Path.Combine(scriptDir, "run_rc_gate.bat")))
            {
                return new DirectoryInfo(scriptDir);
            }
            if (File.Exists(Path.Combine(directory.FullName, "run_rc_gate.bat")))
            {
                return directory;
            }
        }
        return null;
    }

    private static bool ReportMatchesTarget(FileInfo report, string target)
    {
        try
        {
            foreach (var line in File.ReadLines(report.FullName).Take(32))
            {
                if (line.StartsWith("Target:", StringComparison.OrdinalIgnoreCase))
                {
                    var reportTarget = line["Target:".Length..].Trim();
                    return string.Equals(reportTarget, target, StringComparison.OrdinalIgnoreCase);
                }
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    private static string FindDefaultBackgroundFilter()
    {
        var scriptDir = FindRepoScriptDirectory();
        if (scriptDir is null)
        {
            return "background_filter_example.txt";
        }

        var path = Path.Combine(scriptDir.FullName, "background_filter_example.txt");
        return File.Exists(path) ? path : "background_filter_example.txt";
    }

    private static IEnumerable<DirectoryInfo> WalkUp(DirectoryInfo start)
    {
        for (var current = start; current is not null; current = current.Parent)
        {
            yield return current;
        }
    }

    private static string FormatArgumentsForLog(IEnumerable<string> arguments)
    {
        return string.Join(" ", arguments.Select(Quote));
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private static double Smooth(double current, double next)
    {
        return (current * 0.7) + (next * 0.3);
    }

    private static double? TryReadNumberAfter(string line, string marker)
    {
        var index = line.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        var start = index + marker.Length;
        while (start < line.Length && char.IsWhiteSpace(line[start]))
        {
            ++start;
        }

        var end = start;
        while (end < line.Length && (char.IsDigit(line[end]) || line[end] == '.'))
        {
            ++end;
        }

        return end > start && double.TryParse(line[start..end], NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            ? value
            : null;
    }

    private static double? TryReadTrailingNumber(string line)
    {
        var end = line.Length - 1;
        while (end >= 0 && !char.IsDigit(line[end]))
        {
            --end;
        }
        if (end < 0)
        {
            return null;
        }

        var start = end;
        while (start >= 0 && char.IsDigit(line[start]))
        {
            --start;
        }

        return double.TryParse(line[(start + 1)..(end + 1)], NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            ? value
            : null;
    }
}
