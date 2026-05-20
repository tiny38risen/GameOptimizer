using System.Diagnostics;
using System.Text;

namespace GameOptimizer.UI;

public sealed class MainForm : Form
{
    private readonly ComboBox targetCombo = new();
    private readonly Button refreshButton = new();
    private readonly RadioButton dryRunRadio = new();
    private readonly RadioButton softApplyRadio = new();
    private readonly RadioButton applyRadio = new();
    private readonly CheckBox threadDetailCheck = new();
    private readonly CheckBox backgroundDetailCheck = new();
    private readonly CheckBox latencyPingCheck = new();
    private readonly TextBox latencyPingText = new();
    private readonly CheckBox backgroundFilterCheck = new();
    private readonly TextBox backgroundFilterText = new();
    private readonly Button browseFilterButton = new();
    private readonly NumericUpDown runtimeSecondsBox = new();
    private readonly Button startButton = new();
    private readonly Button stopButton = new();
    private readonly Button evidenceFolderButton = new();
    private readonly Button latestReportButton = new();
    private readonly Label statusValue = new();
    private readonly Label enginePathValue = new();
    private readonly RichTextBox logBox = new();

    private Process? runningProcess;

    public MainForm()
    {
        Text = "GameOptimizer v3.0 운영 콘솔";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(1080, 720);
        Size = new Size(1180, 780);
        Font = new Font("Segoe UI", 10F);

        BuildLayout();
        RefreshProcessList();
        UpdateEnginePathLabel();
        UpdateControlState(false);
    }

    private void BuildLayout()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            Padding = new Padding(14),
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 360));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(root);

        var left = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 8,
        };
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(left, 0, 0);

        var title = new Label
        {
            Text = "GameOptimizer v3.0",
            Dock = DockStyle.Top,
            Font = new Font(Font.FontFamily, 18F, FontStyle.Bold),
            AutoSize = true,
            Margin = new Padding(0, 0, 0, 12),
        };
        left.Controls.Add(title);

        left.Controls.Add(CreateTargetPanel());
        left.Controls.Add(CreateModePanel());
        left.Controls.Add(CreateOptionsPanel());
        left.Controls.Add(CreateActionPanel());
        left.Controls.Add(CreateStatusPanel());

        var right = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
        };
        right.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        right.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(right, 1, 0);

        var logHeader = new Label
        {
            Text = "실행 로그",
            Dock = DockStyle.Top,
            Font = new Font(Font.FontFamily, 12F, FontStyle.Bold),
            AutoSize = true,
            Margin = new Padding(14, 0, 0, 8),
        };
        right.Controls.Add(logHeader);

        logBox.Dock = DockStyle.Fill;
        logBox.ReadOnly = true;
        logBox.BackColor = Color.FromArgb(18, 20, 24);
        logBox.ForeColor = Color.Gainsboro;
        logBox.BorderStyle = BorderStyle.FixedSingle;
        logBox.Font = new Font("Consolas", 10F);
        logBox.Margin = new Padding(14, 0, 0, 0);
        right.Controls.Add(logBox);
    }

    private Control CreateTargetPanel()
    {
        var panel = CreateGroup("대상 프로세스");
        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 2 };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        targetCombo.Dock = DockStyle.Fill;
        targetCombo.DropDownStyle = ComboBoxStyle.DropDown;
        table.Controls.Add(targetCombo, 0, 0);

        refreshButton.Text = "새로고침";
        refreshButton.AutoSize = true;
        refreshButton.Click += (_, _) => RefreshProcessList();
        table.Controls.Add(refreshButton, 1, 0);

        enginePathValue.Text = "엔진: 확인 중";
        enginePathValue.AutoSize = true;
        enginePathValue.ForeColor = Color.DimGray;
        enginePathValue.Margin = new Padding(0, 8, 0, 0);
        table.SetColumnSpan(enginePathValue, 2);
        table.Controls.Add(enginePathValue, 0, 1);
        return panel;
    }

    private Control CreateModePanel()
    {
        var panel = CreateGroup("실행 모드");
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true };
        dryRunRadio.Text = "드라이런";
        softApplyRadio.Text = "소프트 적용";
        applyRadio.Text = "실제 적용";
        dryRunRadio.Checked = true;
        applyRadio.CheckedChanged += (_, _) => UpdateApplyWarning();
        flow.Controls.AddRange(new Control[] { dryRunRadio, softApplyRadio, applyRadio });
        panel.Controls.Add(flow);
        return panel;
    }

    private Control CreateOptionsPanel()
    {
        var panel = CreateGroup("옵션");
        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 6, AutoSize = true };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        threadDetailCheck.Text = "스레드 상세 로그";
        table.SetColumnSpan(threadDetailCheck, 3);
        table.Controls.Add(threadDetailCheck, 0, 0);

        backgroundDetailCheck.Text = "백그라운드 상세 로그";
        table.SetColumnSpan(backgroundDetailCheck, 3);
        table.Controls.Add(backgroundDetailCheck, 0, 1);

        latencyPingCheck.Text = "지연시간 Ping";
        latencyPingText.Text = "8.8.8.8";
        latencyPingText.Dock = DockStyle.Fill;
        table.Controls.Add(latencyPingCheck, 0, 2);
        table.Controls.Add(latencyPingText, 1, 2);

        backgroundFilterCheck.Text = "백그라운드 필터";
        backgroundFilterText.Dock = DockStyle.Fill;
        backgroundFilterText.Text = FindDefaultBackgroundFilter();
        browseFilterButton.Text = "...";
        browseFilterButton.Width = 36;
        browseFilterButton.Click += (_, _) => BrowseBackgroundFilter();
        table.Controls.Add(backgroundFilterCheck, 0, 3);
        table.Controls.Add(backgroundFilterText, 1, 3);
        table.Controls.Add(browseFilterButton, 2, 3);

        var runtimeLabel = new Label { Text = "최대 실행 시간(초)", AutoSize = true, Anchor = AnchorStyles.Left };
        runtimeSecondsBox.Minimum = 5;
        runtimeSecondsBox.Maximum = 3600;
        runtimeSecondsBox.Value = 60;
        runtimeSecondsBox.Increment = 5;
        runtimeSecondsBox.Dock = DockStyle.Left;
        table.Controls.Add(runtimeLabel, 0, 4);
        table.Controls.Add(runtimeSecondsBox, 1, 4);
        return panel;
    }

    private Control CreateActionPanel()
    {
        var panel = CreateGroup("제어");
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true };
        startButton.Text = "시작";
        startButton.Width = 90;
        startButton.Click += async (_, _) => await StartEngineAsync();
        stopButton.Text = "중지";
        stopButton.Width = 90;
        stopButton.Click += (_, _) => StopEngine();
        evidenceFolderButton.Text = "증거 폴더";
        evidenceFolderButton.Width = 100;
        evidenceFolderButton.Click += (_, _) => OpenEvidenceFolder();
        latestReportButton.Text = "최근 리포트";
        latestReportButton.Width = 105;
        latestReportButton.Click += (_, _) => OpenLatestReport();
        flow.Controls.AddRange(new Control[] { startButton, stopButton, evidenceFolderButton, latestReportButton });
        panel.Controls.Add(flow);
        return panel;
    }

    private Control CreateStatusPanel()
    {
        var panel = CreateGroup("상태");
        statusValue.Text = "대기 중";
        statusValue.AutoSize = true;
        statusValue.Font = new Font(Font.FontFamily, 13F, FontStyle.Bold);
        statusValue.ForeColor = Color.DimGray;
        panel.Controls.Add(statusValue);
        return panel;
    }

    private static GroupBox CreateGroup(string text)
    {
        return new GroupBox
        {
            Text = text,
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(12),
            Margin = new Padding(0, 0, 0, 12),
        };
    }

    private void RefreshProcessList()
    {
        var previous = targetCombo.Text;
        targetCombo.Items.Clear();
        foreach (var name in Process.GetProcesses()
                     .Select(p => SafeProcessName(p))
                     .Where(name => !string.IsNullOrWhiteSpace(name))
                     .Distinct(StringComparer.OrdinalIgnoreCase)
                     .OrderBy(name => name, StringComparer.OrdinalIgnoreCase))
        {
            targetCombo.Items.Add(name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? name : $"{name}.exe");
        }
        targetCombo.Text = string.IsNullOrWhiteSpace(previous) ? "notepad.exe" : previous;
    }

    private static string SafeProcessName(Process process)
    {
        try
        {
            return process.ProcessName;
        }
        catch
        {
            return string.Empty;
        }
    }

    private async Task StartEngineAsync()
    {
        if (runningProcess is not null)
        {
            return;
        }

        var target = targetCombo.Text.Trim();
        if (string.IsNullOrWhiteSpace(target))
        {
            MessageBox.Show("대상 프로세스를 선택하거나 입력하세요.", "대상 필요", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (applyRadio.Checked)
        {
            var confirm = MessageBox.Show(
                "실제 적용 모드는 대상 스레드 우선순위와 affinity를 변경할 수 있습니다.\n드라이런이나 소프트 적용 로그가 정상일 때만 진행하세요.",
                "실제 적용 확인",
                MessageBoxButtons.OKCancel,
                MessageBoxIcon.Warning);
            if (confirm != DialogResult.OK)
            {
                return;
            }
        }

        var enginePath = FindEnginePath();
        if (enginePath is null)
        {
            MessageBox.Show("GameOptimizer.exe를 찾지 못했습니다. Release 빌드를 먼저 생성하세요.", "엔진 없음", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        ClearLog();
        SetStatus("실행 중", Color.RoyalBlue);
        UpdateControlState(true);

        var psi = new ProcessStartInfo
        {
            FileName = enginePath.FullName,
            Arguments = BuildArguments(target),
            WorkingDirectory = enginePath.DirectoryName ?? AppContext.BaseDirectory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, e) => AppendLogLine(e.Data);
        process.ErrorDataReceived += (_, e) => AppendLogLine(e.Data);
        process.Exited += (_, _) => BeginInvoke(new Action(() =>
        {
            var exitCode = process.ExitCode;
            process.Dispose();
            runningProcess = null;
            UpdateControlState(false);
            SetStatus(exitCode == 0 ? "PASS" : "BLOCKER", exitCode == 0 ? Color.SeaGreen : Color.Firebrick);
            AppendLogLine(exitCode == 0
                ? "[PASS] UI: 엔진 실행이 정상 종료되었습니다."
                : $"[BLOCKER] UI: 엔진 종료 코드 {exitCode}");
        }));

        try
        {
            runningProcess = process;
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            AppendLogLine($"[INFO] UI: {enginePath.FullName}");
            AppendLogLine($"[INFO] UI: GameOptimizer.exe {psi.Arguments}");
            await Task.CompletedTask;
        }
        catch (Exception ex)
        {
            runningProcess = null;
            process.Dispose();
            UpdateControlState(false);
            SetStatus("BLOCKER", Color.Firebrick);
            AppendLogLine($"[BLOCKER] UI: 실행 실패 - {ex.Message}");
        }
    }

    private string BuildArguments(string target)
    {
        var args = new List<string> { Quote(target) };
        if (dryRunRadio.Checked)
        {
            args.Add("--dry-run");
        }
        if (applyRadio.Checked)
        {
            args.Add("--apply");
        }
        if (threadDetailCheck.Checked)
        {
            args.Add("--thread-detail-log");
        }
        if (backgroundDetailCheck.Checked)
        {
            args.Add("--background-detail-log");
        }
        if (latencyPingCheck.Checked && !string.IsNullOrWhiteSpace(latencyPingText.Text))
        {
            args.Add("--latency-ping");
            args.Add(Quote(latencyPingText.Text.Trim()));
        }
        if (backgroundFilterCheck.Checked && !string.IsNullOrWhiteSpace(backgroundFilterText.Text))
        {
            args.Add("--background-filter");
            args.Add(Quote(backgroundFilterText.Text.Trim()));
        }
        args.Add("--max-runtime-seconds");
        args.Add(((int)runtimeSecondsBox.Value).ToString());
        return string.Join(" ", args);
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private void StopEngine()
    {
        if (runningProcess is null)
        {
            return;
        }

        try
        {
            runningProcess.Kill(entireProcessTree: true);
            AppendLogLine("[WARN] UI: 사용자가 실행을 중지했습니다.");
        }
        catch (Exception ex)
        {
            AppendLogLine($"[WARN] UI: 중지 실패 - {ex.Message}");
        }
    }

    private void AppendLogLine(string? line)
    {
        if (string.IsNullOrEmpty(line))
        {
            return;
        }

        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => AppendLogLine(line)));
            return;
        }

        var color = Color.Gainsboro;
        if (line.Contains("[BLOCKER]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[ERROR]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase))
        {
            color = Color.LightCoral;
            SetStatus("BLOCKER", Color.Firebrick);
        }
        else if (line.Contains("[WARN]", StringComparison.OrdinalIgnoreCase))
        {
            color = Color.Khaki;
            if (statusValue.Text != "BLOCKER")
            {
                SetStatus("WARN", Color.DarkGoldenrod);
            }
        }
        else if (line.Contains("[PASS]", StringComparison.OrdinalIgnoreCase))
        {
            color = Color.LightGreen;
        }

        logBox.SelectionStart = logBox.TextLength;
        logBox.SelectionLength = 0;
        logBox.SelectionColor = color;
        logBox.AppendText(line + Environment.NewLine);
        logBox.SelectionColor = logBox.ForeColor;
        logBox.ScrollToCaret();
    }

    private void ClearLog()
    {
        logBox.Clear();
    }

    private void SetStatus(string text, Color color)
    {
        statusValue.Text = text;
        statusValue.ForeColor = color;
    }

    private void UpdateControlState(bool running)
    {
        startButton.Enabled = !running;
        stopButton.Enabled = running;
        targetCombo.Enabled = !running;
        refreshButton.Enabled = !running;
        dryRunRadio.Enabled = !running;
        softApplyRadio.Enabled = !running;
        applyRadio.Enabled = !running;
    }

    private void UpdateEnginePathLabel()
    {
        var path = FindEnginePath();
        enginePathValue.Text = path is null ? "엔진: 찾을 수 없음" : $"엔진: {path.FullName}";
        enginePathValue.ForeColor = path is null ? Color.Firebrick : Color.DimGray;
    }

    private void UpdateApplyWarning()
    {
        if (applyRadio.Checked)
        {
            SetStatus("주의: 실제 적용", Color.DarkGoldenrod);
        }
        else if (runningProcess is null)
        {
            SetStatus("대기 중", Color.DimGray);
        }
    }

    private void BrowseBackgroundFilter()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "백그라운드 필터 파일 선택",
            Filter = "텍스트 파일 (*.txt)|*.txt|모든 파일 (*.*)|*.*",
            FileName = Path.GetFileName(backgroundFilterText.Text),
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            backgroundFilterText.Text = dialog.FileName;
            backgroundFilterCheck.Checked = true;
        }
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
            MessageBox.Show("아직 evidence report가 없습니다.", "리포트 없음", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        var report = evidence.GetFiles("rc_evidence_report.txt", SearchOption.AllDirectories)
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .FirstOrDefault();
        if (report is null)
        {
            MessageBox.Show("아직 rc_evidence_report.txt가 없습니다.", "리포트 없음", MessageBoxButtons.OK, MessageBoxIcon.Information);
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

    private static IEnumerable<DirectoryInfo> WalkUp(DirectoryInfo start)
    {
        for (var current = start; current is not null; current = current.Parent)
        {
            yield return current;
        }
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
}
