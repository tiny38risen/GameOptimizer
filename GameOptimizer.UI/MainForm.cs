using System.Diagnostics;
using System.Text;

namespace GameOptimizer.UI;

public sealed partial class MainForm : Form
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
    private readonly Button restoreButton = new();
    private readonly Button evidenceFolderButton = new();
    private readonly Button latestReportButton = new();
    private readonly Button detailsToggleButton = new();
    private readonly Label gameStateValue = new();
    private readonly Label statusValue = new();
    private readonly Label optimizeStateValue = new();
    private readonly Label recoveryStateValue = new();
    private readonly Label enginePathValue = new();
    private readonly TableLayoutPanel detailsPanel = new();
    private readonly RichTextBox logBox = new();

    private Process? runningProcess;
    private bool detailsExpanded = true;

    private sealed class ProcessListItem
    {
        public required string ExeName { get; init; }
        public required int ProcessId { get; init; }
        public string WindowTitle { get; init; } = string.Empty;

        public override string ToString()
        {
            return string.IsNullOrWhiteSpace(WindowTitle)
                ? $"{ExeName}  (PID {ProcessId})"
                : $"{ExeName}  (PID {ProcessId}) - {WindowTitle}";
        }
    }

    public MainForm()
    {
        InitializeComponent();
        RefreshProcessList();
        UpdateEnginePathLabel();
        UpdateSummaryState("대기 중", Color.DimGray, "최적화 대기 중");
        UpdateControlState(false);
    }

    private void BuildLayout()
    {
        BackColor = Color.FromArgb(245, 246, 248);

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            Padding = new Padding(18),
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 420));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(root);

        var left = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 4,
            AutoScroll = true,
        };
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(left, 0, 0);

        left.Controls.Add(CreateSummaryPanel());
        left.Controls.Add(CreateDetailsToggle());

        detailsPanel.Dock = DockStyle.Top;
        detailsPanel.AutoSize = true;
        detailsPanel.ColumnCount = 1;
        detailsPanel.RowCount = 4;
        detailsPanel.Controls.Add(CreateDetailSection(
            "CPU",
            new[] { "사용 코어 : 4", "보조 코어 : 6", "CPU 상태 : 정상" },
            new[] { "Processor Group : 0", "Primary Core : 4", "Fallback Core : 6", "SMT 분리 : 적용됨", "CCX 최적화 : 적용됨" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "스레드",
            new[] { "게임 메인 스레드 감지됨", "상태 : 안정적", "불필요한 이동 없음" },
            new[] { "메인 스레드 ID : 1234", "EMA 점수 : 91", "Stickiness : 4초", "Migration : 0회" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "네트워크",
            new[] { "네트워크 상태 : 좋음", "지연 변동 : 낮음", "간섭 신호 : 없음" },
            new[] { "RTT : 5ms", "RTT 변동 : 1.2ms", "DPC 발생 : 0회", "IRQ 재배치 : 없음" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "안전 복구",
            new[] { "복구 정보 저장 완료", "자동 복구 가능", "마지막 검사 : 정상" },
            new[] { "Affinity 백업 : 완료", "Priority 백업 : 완료", "ApplyGuard : 정상", "Rollback 준비 : 완료" }));
        left.Controls.Add(detailsPanel);
        left.Controls.Add(CreateEngineOptionsPanel());

        var right = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            Margin = new Padding(18, 0, 0, 0),
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
            Margin = new Padding(0, 0, 0, 8),
        };
        right.Controls.Add(logHeader);

        logBox.Dock = DockStyle.Fill;
        logBox.ReadOnly = true;
        logBox.BackColor = Color.FromArgb(18, 20, 24);
        logBox.ForeColor = Color.Gainsboro;
        logBox.BorderStyle = BorderStyle.None;
        logBox.Font = new Font("Consolas", 10F);
        right.Controls.Add(logBox);
    }

    private Control CreateSummaryPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(18);

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 6, AutoSize = true };
        panel.Controls.Add(table);

        table.Controls.Add(new Label
        {
            Text = "GameOptimizer",
            AutoSize = true,
            Font = new Font(Font.FontFamily, 20F, FontStyle.Bold),
            Margin = new Padding(0, 0, 0, 14),
        });

        gameStateValue.Text = "마비노기 실행 중";
        gameStateValue.AutoSize = true;
        gameStateValue.Font = new Font(Font.FontFamily, 12F, FontStyle.Bold);
        gameStateValue.Margin = new Padding(0, 0, 0, 18);
        table.Controls.Add(gameStateValue);

        statusValue.Text = "상태 : 안정적";
        statusValue.AutoSize = true;
        statusValue.Font = new Font(Font.FontFamily, 11.5F, FontStyle.Bold);
        table.Controls.Add(statusValue);

        optimizeStateValue.Text = "최적화 적용 중";
        optimizeStateValue.AutoSize = true;
        optimizeStateValue.Margin = new Padding(0, 8, 0, 0);
        table.Controls.Add(optimizeStateValue);

        recoveryStateValue.Text = "자동 복구 가능";
        recoveryStateValue.AutoSize = true;
        recoveryStateValue.Margin = new Padding(0, 8, 0, 18);
        table.Controls.Add(recoveryStateValue);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Margin = new Padding(0) };
        startButton.Text = "최적화 시작";
        startButton.Width = 118;
        startButton.Height = 36;
        startButton.Click += async (_, _) => await StartEngineAsync();
        restoreButton.Text = "원상복구";
        restoreButton.Width = 100;
        restoreButton.Height = 36;
        restoreButton.Click += (_, _) => StopEngine();
        actions.Controls.AddRange(new Control[] { startButton, restoreButton });
        table.Controls.Add(actions);

        return panel;
    }

    private Control CreateDetailsToggle()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(14, 10, 14, 10);
        detailsToggleButton.Dock = DockStyle.Fill;
        detailsToggleButton.FlatStyle = FlatStyle.Flat;
        detailsToggleButton.FlatAppearance.BorderSize = 0;
        detailsToggleButton.TextAlign = ContentAlignment.MiddleLeft;
        detailsToggleButton.Text = "▼ 상세 정보";
        detailsToggleButton.Height = 36;
        detailsToggleButton.Click += (_, _) => ToggleDetails();
        panel.Controls.Add(detailsToggleButton);
        return panel;
    }

    private Control CreateDetailSection(string title, string[] summaryLines, string[] advancedLines)
    {
        var panel = CreateCard();
        panel.Padding = new Padding(14);

        var table = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, RowCount = 3, AutoSize = true };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var header = new Label
        {
            Text = $"▼ {title}",
            AutoSize = true,
            Font = new Font(Font.FontFamily, 11.5F, FontStyle.Bold),
            Anchor = AnchorStyles.Left,
        };
        table.Controls.Add(header, 0, 0);

        var advancedButton = new Button
        {
            Text = "고급",
            Width = 64,
            Height = 30,
            Anchor = AnchorStyles.Right,
        };
        table.Controls.Add(advancedButton, 1, 0);

        var summary = CreateTextStack(summaryLines, false);
        summary.Margin = new Padding(0, 10, 0, 0);
        table.SetColumnSpan(summary, 2);
        table.Controls.Add(summary, 0, 1);

        var advanced = CreateTextStack(advancedLines, true);
        advanced.Visible = false;
        advanced.Margin = new Padding(0, 12, 0, 0);
        table.SetColumnSpan(advanced, 2);
        table.Controls.Add(advanced, 0, 2);

        advancedButton.Click += (_, _) => advanced.Visible = !advanced.Visible;

        return panel;
    }

    private Control CreateTextStack(IEnumerable<string> lines, bool developerInfo)
    {
        var stack = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 1, AutoSize = true };
        if (developerInfo)
        {
            stack.BackColor = Color.FromArgb(32, 36, 43);
            stack.Padding = new Padding(12);
        }

        foreach (var line in lines)
        {
            stack.Controls.Add(new Label
            {
                Text = line,
                AutoSize = true,
                ForeColor = developerInfo ? Color.Gainsboro : Color.FromArgb(36, 39, 45),
                Font = developerInfo ? new Font("Consolas", 9.5F) : Font,
                Margin = new Padding(0, 2, 0, 4),
            });
        }

        return stack;
    }

    private Control CreateEngineOptionsPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(14);

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 9, AutoSize = true };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var heading = new Label
        {
            Text = "엔진 설정",
            AutoSize = true,
            Font = new Font(Font.FontFamily, 11.5F, FontStyle.Bold),
            Margin = new Padding(0, 0, 0, 10),
        };
        table.SetColumnSpan(heading, 3);
        table.Controls.Add(heading, 0, 0);

        var targetLabel = new Label { Text = "대상", AutoSize = true, Anchor = AnchorStyles.Left };
        targetCombo.Dock = DockStyle.Fill;
        targetCombo.DropDownStyle = ComboBoxStyle.DropDownList;
        refreshButton.Text = "새로고침";
        refreshButton.AutoSize = true;
        refreshButton.Click += (_, _) => RefreshProcessList();
        table.Controls.Add(targetLabel, 0, 1);
        table.Controls.Add(targetCombo, 1, 1);
        table.Controls.Add(refreshButton, 2, 1);

        enginePathValue.Text = "엔진: 확인 중";
        enginePathValue.AutoSize = true;
        enginePathValue.ForeColor = Color.DimGray;
        enginePathValue.Margin = new Padding(0, 8, 0, 10);
        table.SetColumnSpan(enginePathValue, 3);
        table.Controls.Add(enginePathValue, 0, 2);

        var modeFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true };
        dryRunRadio.Text = "드라이런";
        softApplyRadio.Text = "소프트 적용";
        applyRadio.Text = "실제 적용";
        dryRunRadio.Checked = true;
        applyRadio.CheckedChanged += (_, _) => UpdateApplyWarning();
        modeFlow.Controls.AddRange(new Control[] { dryRunRadio, softApplyRadio, applyRadio });
        table.SetColumnSpan(modeFlow, 3);
        table.Controls.Add(modeFlow, 0, 3);

        threadDetailCheck.Text = "스레드 상세 로그";
        table.SetColumnSpan(threadDetailCheck, 3);
        table.Controls.Add(threadDetailCheck, 0, 4);

        backgroundDetailCheck.Text = "백그라운드 상세 로그";
        table.SetColumnSpan(backgroundDetailCheck, 3);
        table.Controls.Add(backgroundDetailCheck, 0, 5);

        latencyPingCheck.Text = "지연시간 Ping";
        latencyPingText.Text = "8.8.8.8";
        latencyPingText.Dock = DockStyle.Fill;
        table.Controls.Add(latencyPingCheck, 0, 6);
        table.Controls.Add(latencyPingText, 1, 6);

        backgroundFilterCheck.Text = "백그라운드 필터";
        backgroundFilterText.Dock = DockStyle.Fill;
        backgroundFilterText.Text = FindDefaultBackgroundFilter();
        browseFilterButton.Text = "...";
        browseFilterButton.Width = 36;
        browseFilterButton.Click += (_, _) => BrowseBackgroundFilter();
        table.Controls.Add(backgroundFilterCheck, 0, 7);
        table.Controls.Add(backgroundFilterText, 1, 7);
        table.Controls.Add(browseFilterButton, 2, 7);

        var runtimeLabel = new Label { Text = "최대 실행 시간(초)", AutoSize = true, Anchor = AnchorStyles.Left };
        runtimeSecondsBox.Minimum = 5;
        runtimeSecondsBox.Maximum = 3600;
        runtimeSecondsBox.Value = 60;
        runtimeSecondsBox.Increment = 5;
        runtimeSecondsBox.Dock = DockStyle.Left;
        table.Controls.Add(runtimeLabel, 0, 8);
        table.Controls.Add(runtimeSecondsBox, 1, 8);

        var utilityFlow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Margin = new Padding(0, 12, 0, 0) };
        evidenceFolderButton.Text = "증거 폴더";
        evidenceFolderButton.Width = 100;
        evidenceFolderButton.Click += (_, _) => OpenEvidenceFolder();
        latestReportButton.Text = "최근 리포트";
        latestReportButton.Width = 105;
        latestReportButton.Click += (_, _) => OpenLatestReport();
        utilityFlow.Controls.AddRange(new Control[] { evidenceFolderButton, latestReportButton });
        table.SetColumnSpan(utilityFlow, 3);
        table.Controls.Add(utilityFlow, 0, 9);

        return panel;
    }

    private static Panel CreateCard()
    {
        return new Panel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            BackColor = Color.White,
            Margin = new Padding(0, 0, 0, 12),
        };
    }

    private void ToggleDetails()
    {
        detailsExpanded = !detailsExpanded;
        detailsPanel.Visible = detailsExpanded;
        detailsToggleButton.Text = detailsExpanded ? "▼ 상세 정보" : "▶ 상세 정보";
    }

    private void RefreshProcessList()
    {
        var previous = GetSelectedTargetName();
        targetCombo.Items.Clear();
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
            targetCombo.Items.Add(item);
        }

        var previousMatch = items.FindIndex(item => string.Equals(item.ExeName, previous, StringComparison.OrdinalIgnoreCase));
        var mabinogiMatch = items.FindIndex(IsLikelyMabinogi);
        if (previousMatch >= 0)
        {
            targetCombo.SelectedIndex = previousMatch;
        }
        else if (mabinogiMatch >= 0)
        {
            targetCombo.SelectedIndex = mabinogiMatch;
        }
        else if (targetCombo.Items.Count > 0)
        {
            targetCombo.SelectedIndex = 0;
        }

        gameStateValue.Text = mabinogiMatch >= 0 ? "마비노기 실행 중" : "게임 대기 중";
    }

    private static bool IsLikelyMabinogi(ProcessListItem item)
    {
        return item.ExeName.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("마비노기", StringComparison.OrdinalIgnoreCase);
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

    private async Task StartEngineAsync()
    {
        if (runningProcess is not null)
        {
            return;
        }

        var target = GetSelectedTargetName();
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
        UpdateSummaryState("실행 중", Color.RoyalBlue, "최적화 적용 중");
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
            UpdateSummaryState(exitCode == 0 ? "안정적" : "점검 필요", exitCode == 0 ? Color.SeaGreen : Color.Firebrick, exitCode == 0 ? "최적화 완료" : "최적화 중단");
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
            UpdateSummaryState("점검 필요", Color.Firebrick, "최적화 실패");
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

    private string GetSelectedTargetName()
    {
        if (targetCombo.SelectedItem is ProcessListItem item)
        {
            return item.ExeName;
        }
        return targetCombo.Text.Trim();
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private void StopEngine()
    {
        if (runningProcess is null)
        {
            UpdateSummaryState("안정적", Color.SeaGreen, "원상복구 완료");
            AppendLogLine("[INFO] UI: 실행 중인 엔진이 없어 상태만 복구 완료로 표시했습니다.");
            return;
        }

        try
        {
            runningProcess.Kill(entireProcessTree: true);
            UpdateSummaryState("복구 중", Color.DarkGoldenrod, "원상복구 실행 중");
            AppendLogLine("[WARN] UI: 사용자가 원상복구를 요청했습니다.");
        }
        catch (Exception ex)
        {
            AppendLogLine($"[WARN] UI: 원상복구 실패 - {ex.Message}");
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
            UpdateSummaryState("점검 필요", Color.Firebrick, "최적화 중단");
        }
        else if (line.Contains("[WARN]", StringComparison.OrdinalIgnoreCase))
        {
            color = Color.Khaki;
            if (!statusValue.Text.Contains("점검 필요", StringComparison.OrdinalIgnoreCase))
            {
                UpdateSummaryState("주의", Color.DarkGoldenrod, optimizeStateValue.Text);
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

    private void UpdateSummaryState(string state, Color color, string optimizeText)
    {
        statusValue.Text = $"상태 : {state}";
        statusValue.ForeColor = color;
        optimizeStateValue.Text = optimizeText;
        recoveryStateValue.Text = "자동 복구 가능";
    }

    private void UpdateControlState(bool running)
    {
        startButton.Enabled = !running;
        restoreButton.Enabled = true;
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
            UpdateSummaryState("주의: 실제 적용", Color.DarkGoldenrod, "실제 적용 준비");
        }
        else if (runningProcess is null)
        {
            UpdateSummaryState("대기 중", Color.DimGray, "최적화 대기 중");
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
