using System.Diagnostics;
using System.Text;

namespace GameOptimizer.UI;

public sealed partial class MainForm : Form
{
    private const int FormWidth = 520;

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
    private readonly Button evidenceFolderButton = new();
    private readonly Button latestReportButton = new();
    private readonly Button settingsToggleButton = new();
    private readonly Button detailsToggleButton = new();
    private readonly CheckBox runtimeLimitCheck = new();
    private readonly Label gameStateValue = new();
    private readonly Label statusValue = new();
    private readonly Label optimizeStateValue = new();
    private readonly Label recoveryStateValue = new();
    private readonly Label enginePathValue = new();
    private readonly Label modeDescriptionValue = new();
    private readonly Label runtimeDescriptionValue = new();
    private readonly TableLayoutPanel contentPanel = new();
    private readonly TableLayoutPanel settingsPanel = new();
    private readonly TableLayoutPanel detailsPanel = new();
    private readonly List<string> hiddenLogLines = new();

    private Process? runningProcess;
    private bool settingsExpanded;
    private bool detailsExpanded;

    private sealed class CardPanel : Panel
    {
        public CardPanel()
        {
            DoubleBuffered = true;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using var pen = new Pen(DesignSystem.BorderColor);
            var rectangle = ClientRectangle;
            rectangle.Width -= 1;
            rectangle.Height -= 1;
            e.Graphics.DrawRectangle(pen, rectangle);
        }
    }

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

    private sealed class TargetSelection
    {
        public required string ExeName { get; init; }
        public int? ProcessId { get; init; }
    }

    public MainForm()
    {
        InitializeComponent();
        RefreshProcessList();
        UpdateEnginePathLabel();
        UpdateSummaryState("대기", DesignSystem.TextMuted, "최적화 대기 중");
        UpdateModeDescription();
        UpdateRuntimeLimitState();
        UpdateControlState(false);
        AdjustFormHeight();
    }

    private void BuildLayout()
    {
        BackColor = DesignSystem.BgColor;

        var scrollHost = new Panel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            Padding = DesignSystem.CardPadding,
            BackColor = DesignSystem.BgColor,
        };
        Controls.Add(scrollHost);

        contentPanel.Dock = DockStyle.Top;
        contentPanel.AutoSize = true;
        contentPanel.ColumnCount = 1;
        contentPanel.RowCount = 3;
        contentPanel.BackColor = DesignSystem.BgColor;
        contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        scrollHost.Controls.Add(contentPanel);

        contentPanel.Controls.Add(CreateSummaryPanel(), 0, 0);
        contentPanel.Controls.Add(CreateStatusPanel(), 0, 1);

        settingsPanel.Dock = DockStyle.Top;
        settingsPanel.AutoSize = true;
        settingsPanel.ColumnCount = 1;
        settingsPanel.RowCount = 3;
        settingsPanel.BackColor = DesignSystem.BgColor;
        settingsPanel.Visible = false;
        settingsPanel.Controls.Add(CreateEngineOptionsPanel(), 0, 0);
        settingsPanel.Controls.Add(CreateDetailsToggle(), 0, 1);

        detailsPanel.Dock = DockStyle.Top;
        detailsPanel.AutoSize = true;
        detailsPanel.ColumnCount = 1;
        detailsPanel.RowCount = 4;
        detailsPanel.Visible = false;
        detailsPanel.Controls.Add(CreateDetailSection(
            "CPU",
            new[] { "사용 코어 : 4 / 보조 코어 : 6", "CPU 상태 : 정상" },
            new[] { "Processor Group : 0", "Primary Core : 4", "Fallback Core : 6", "SMT 분리 : 적용됨", "CCX 최적화 : 적용됨" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "스레드",
            new[] { "게임 메인 스레드 감지됨 / 상태 : 안정적", "불필요한 이동 없음" },
            new[] { "메인 스레드 ID : 1234", "EMA 점수 : 91", "Stickiness : 4초", "Migration : 0회" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "네트워크",
            new[] { "네트워크 상태 : 좋음", "지연 변동 : 낮음", "간섭 신호 : 없음" },
            new[] { "RTT : 5ms", "RTT 변동 : 1.2ms", "DPC 발생 : 0회", "IRQ 재배치 : 없음" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "안전 복구",
            new[] { "복구 정보 저장 완료", "자동 복구 가능", "마지막 검사 : 정상" },
            new[] { "Affinity 백업 : 완료", "Priority 백업 : 완료", "ApplyGuard : 정상", "Rollback 준비 : 완료" }));
        settingsPanel.Controls.Add(detailsPanel, 0, 2);
        contentPanel.Controls.Add(settingsPanel, 0, 2);
    }

    private Control CreateSummaryPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        panel.Controls.Add(table);

        table.Controls.Add(new Label
        {
            Text = "Game Optimizer",
            AutoSize = true,
            Font = DesignSystem.FontTitle,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 16),
        });

        var actions = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Margin = new Padding(0) };
        startButton.Text = "[최적화]";
        StyleButton(startButton, primary: true, width: 180);
        startButton.Click += async (_, _) => await ToggleEngineAsync();
        settingsToggleButton.Text = "설정 열기";
        StyleButton(settingsToggleButton, primary: false, width: 116);
        settingsToggleButton.Click += (_, _) => ToggleSettings();
        actions.Controls.AddRange(new Control[] { startButton, settingsToggleButton });
        table.Controls.Add(actions);

        return panel;
    }

    private Control CreateStatusPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        panel.Controls.Add(table);

        gameStateValue.Text = "마비노기 실행 중";
        gameStateValue.AutoSize = true;
        gameStateValue.Font = DesignSystem.FontHeading;
        gameStateValue.ForeColor = DesignSystem.PrimaryColor;
        gameStateValue.Margin = new Padding(0, 0, 0, 14);
        table.Controls.Add(gameStateValue);

        statusValue.Text = "현재 상태 : 대기";
        statusValue.AutoSize = true;
        statusValue.Font = DesignSystem.FontHeading;
        statusValue.ForeColor = DesignSystem.TextMuted;
        table.Controls.Add(statusValue);

        optimizeStateValue.Text = "최적화 적용 중";
        optimizeStateValue.AutoSize = true;
        optimizeStateValue.Font = DesignSystem.FontBody;
        optimizeStateValue.ForeColor = DesignSystem.TextBody;
        optimizeStateValue.Margin = new Padding(0, 8, 0, 0);
        table.Controls.Add(optimizeStateValue);

        recoveryStateValue.Text = "자동 복구 가능";
        recoveryStateValue.AutoSize = true;
        recoveryStateValue.Font = DesignSystem.FontSmall;
        recoveryStateValue.ForeColor = DesignSystem.TextMuted;
        recoveryStateValue.Margin = new Padding(0, 8, 0, 18);
        table.Controls.Add(recoveryStateValue);

        modeDescriptionValue.Text = "현재 모드는 게임 상태를 관찰하고 로그만 남깁니다. 시스템 설정은 바꾸지 않습니다.";
        modeDescriptionValue.AutoSize = true;
        modeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        modeDescriptionValue.Font = DesignSystem.FontSmall;
        modeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        modeDescriptionValue.Margin = new Padding(0, 0, 0, 18);
        table.Controls.Add(modeDescriptionValue);

        return panel;
    }

    private Control CreateDetailsToggle()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(16, 10, 16, 10);
        detailsToggleButton.Dock = DockStyle.Fill;
        detailsToggleButton.FlatStyle = FlatStyle.Flat;
        detailsToggleButton.FlatAppearance.BorderSize = 0;
        detailsToggleButton.Cursor = Cursors.Hand;
        detailsToggleButton.BackColor = DesignSystem.SurfaceColor;
        detailsToggleButton.ForeColor = DesignSystem.PrimaryColor;
        detailsToggleButton.Font = DesignSystem.FontHeading;
        detailsToggleButton.TextAlign = ContentAlignment.MiddleCenter;
        detailsToggleButton.Text = "▼ 모니터링 상세 정보 열기";
        detailsToggleButton.Height = 40;
        detailsToggleButton.Click += (_, _) => ToggleDetails();
        panel.Controls.Add(detailsToggleButton);
        return panel;
    }

    private Control CreateDetailSection(string title, string[] summaryLines, string[] advancedLines)
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, RowCount = 3, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var header = new Label
        {
            Text = $"▼ {title}",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Anchor = AnchorStyles.Left,
        };
        table.Controls.Add(header, 0, 0);

        var advancedButton = new Button
        {
            Text = "고급",
            Anchor = AnchorStyles.Right,
        };
        StyleButton(advancedButton, primary: false, width: 64, height: 30);
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
        var stack = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            ColumnCount = 1,
            AutoSize = true,
            BackColor = developerInfo ? DesignSystem.BgColor : DesignSystem.SurfaceColor,
        };
        if (developerInfo)
        {
            stack.Padding = new Padding(12);
        }

        foreach (var line in lines)
        {
            stack.Controls.Add(new Label
            {
                Text = line,
                AutoSize = true,
                ForeColor = developerInfo ? DesignSystem.PrimaryColor : DesignSystem.TextBody,
                Font = developerInfo ? new Font("Consolas", 9.5F) : DesignSystem.FontBody,
                Margin = new Padding(0, 2, 0, 4),
            });
        }

        return stack;
    }

    private Control CreateEngineOptionsPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 16, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var heading = new Label
        {
            Text = "엔진 설정",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 10),
        };
        table.SetColumnSpan(heading, 3);
        table.Controls.Add(heading, 0, 0);

        var targetLabel = new Label { Text = "대상", AutoSize = true, Anchor = AnchorStyles.Left };
        targetLabel.Font = DesignSystem.FontBody;
        targetLabel.ForeColor = DesignSystem.TextBody;
        targetCombo.Dock = DockStyle.Fill;
        targetCombo.DropDownStyle = ComboBoxStyle.DropDown;
        targetCombo.FlatStyle = FlatStyle.Flat;
        StyleInput(targetCombo);
        refreshButton.Text = "새로고침";
        StyleButton(refreshButton, primary: false, width: 104, height: 30);
        refreshButton.Click += (_, _) => RefreshProcessList();
        table.Controls.Add(targetLabel, 0, 1);
        table.Controls.Add(targetCombo, 1, 1);
        table.Controls.Add(refreshButton, 2, 1);

        enginePathValue.Text = "엔진: 확인 중";
        enginePathValue.AutoSize = true;
        enginePathValue.Font = DesignSystem.FontSmall;
        enginePathValue.ForeColor = DesignSystem.TextMuted;
        enginePathValue.Margin = new Padding(0, 8, 0, 10);
        table.SetColumnSpan(enginePathValue, 3);
        table.Controls.Add(enginePathValue, 0, 2);

        var modeFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
        dryRunRadio.Text = "드라이런";
        softApplyRadio.Text = "소프트 적용";
        applyRadio.Text = "실제 적용";
        StyleOption(dryRunRadio);
        StyleOption(softApplyRadio);
        StyleOption(applyRadio);
        applyRadio.Checked = true;
        dryRunRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        softApplyRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        applyRadio.CheckedChanged += (_, _) => UpdateApplyWarning();
        modeFlow.Controls.AddRange(new Control[] { dryRunRadio, softApplyRadio, applyRadio });
        table.SetColumnSpan(modeFlow, 3);
        table.Controls.Add(modeFlow, 0, 3);

        AddHelpText(table, "기본값은 시간 제한 없이 유지되는 소프트 적용입니다. 드라이런은 관찰만 하고, 실제 적용은 확인창을 거칩니다.", 4);

        threadDetailCheck.Text = "스레드 상세 로깅";
        StyleOption(threadDetailCheck);
        table.SetColumnSpan(threadDetailCheck, 3);
        table.Controls.Add(threadDetailCheck, 0, 5);
        AddHelpText(table, "게임 메인 스레드 감지, 이동 여부, 안정성 판단 근거를 로그에 더 자세히 남깁니다.", 6);

        backgroundDetailCheck.Text = "백그라운드 상세 로깅";
        StyleOption(backgroundDetailCheck);
        table.SetColumnSpan(backgroundDetailCheck, 3);
        table.Controls.Add(backgroundDetailCheck, 0, 7);
        AddHelpText(table, "게임 외 프로세스가 최적화 판단에 영향을 주는지 확인하기 위한 진단 로그입니다.", 8);

        latencyPingCheck.Text = "지연시간 Ping";
        StyleOption(latencyPingCheck);
        latencyPingText.Text = "8.8.8.8";
        latencyPingText.Dock = DockStyle.Fill;
        latencyPingText.BorderStyle = BorderStyle.FixedSingle;
        StyleInput(latencyPingText);
        table.Controls.Add(latencyPingCheck, 0, 9);
        table.Controls.Add(latencyPingText, 1, 9);
        AddHelpText(table, "입력한 주소로 RTT 변동을 관찰합니다. 네트워크 설정을 직접 변경하지 않습니다.", 10);

        backgroundFilterCheck.Text = "백그라운드 필터";
        StyleOption(backgroundFilterCheck);
        backgroundFilterText.Dock = DockStyle.Fill;
        backgroundFilterText.BorderStyle = BorderStyle.FixedSingle;
        backgroundFilterText.Text = FindDefaultBackgroundFilter();
        StyleInput(backgroundFilterText);
        browseFilterButton.Text = "...";
        StyleButton(browseFilterButton, primary: false, width: 36, height: 28);
        browseFilterButton.Click += (_, _) => BrowseBackgroundFilter();
        table.Controls.Add(backgroundFilterCheck, 0, 11);
        table.Controls.Add(backgroundFilterText, 1, 11);
        table.Controls.Add(browseFilterButton, 2, 11);
        AddHelpText(table, "무시하거나 별도 판단할 백그라운드 프로세스 목록 파일입니다.", 12);

        runtimeLimitCheck.Text = "실행 시간 제한";
        StyleOption(runtimeLimitCheck);
        runtimeLimitCheck.CheckedChanged += (_, _) => UpdateRuntimeLimitState();
        runtimeSecondsBox.Minimum = 5;
        runtimeSecondsBox.Maximum = 3600;
        runtimeSecondsBox.Value = 60;
        runtimeSecondsBox.Increment = 5;
        runtimeSecondsBox.Dock = DockStyle.Left;
        runtimeSecondsBox.BackColor = DesignSystem.BgColor;
        runtimeSecondsBox.ForeColor = DesignSystem.TextBody;
        table.Controls.Add(runtimeLimitCheck, 0, 13);
        table.Controls.Add(runtimeSecondsBox, 1, 13);
        runtimeDescriptionValue.Text = "꺼두면 사용자가 종료를 누르거나 엔진이 종료될 때까지 계속 유지됩니다. 켜면 지정한 시간 뒤 종료됩니다.";
        runtimeDescriptionValue.AutoSize = true;
        runtimeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        runtimeDescriptionValue.Font = DesignSystem.FontSmall;
        runtimeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        runtimeDescriptionValue.Margin = new Padding(0, 2, 0, 8);
        table.SetColumnSpan(runtimeDescriptionValue, 3);
        table.Controls.Add(runtimeDescriptionValue, 0, 14);

        var utilityFlow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Margin = new Padding(0, 12, 0, 0) };
        evidenceFolderButton.Text = "로그 폴더";
        StyleButton(evidenceFolderButton, primary: false, width: 104);
        evidenceFolderButton.Click += (_, _) => OpenEvidenceFolder();
        latestReportButton.Text = "최근 리포트";
        StyleButton(latestReportButton, primary: false, width: 116);
        latestReportButton.Click += (_, _) => OpenLatestReport();
        utilityFlow.Controls.AddRange(new Control[] { evidenceFolderButton, latestReportButton });
        table.SetColumnSpan(utilityFlow, 3);
        table.Controls.Add(utilityFlow, 0, 15);

        return panel;
    }

    private static Panel CreateCard()
    {
        return new CardPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            BackColor = DesignSystem.SurfaceColor,
            Margin = new Padding(0, 0, 0, DesignSystem.ControlMargin),
        };
    }

    private static void AddHelpText(TableLayoutPanel table, string text, int row)
    {
        var helpText = CreateHelpText(text);
        table.SetColumnSpan(helpText, 3);
        table.Controls.Add(helpText, 0, row);
    }

    private static Label CreateHelpText(string text)
    {
        return new Label
        {
            Text = text,
            AutoSize = true,
            MaximumSize = new Size(GetTextWrapWidth(), 0),
            Font = DesignSystem.FontSmall,
            ForeColor = DesignSystem.TextMuted,
            Margin = new Padding(0, 0, 0, 8),
        };
    }

    private static int GetTextWrapWidth()
    {
        return FormWidth - (DesignSystem.CardPadding.Horizontal * 3);
    }

    private static void StyleButton(Button button, bool primary, int width, int height = 36)
    {
        button.Width = width;
        button.Height = height;
        button.FlatStyle = FlatStyle.Flat;
        button.FlatAppearance.BorderSize = primary ? 0 : 1;
        button.FlatAppearance.BorderColor = primary ? DesignSystem.PrimaryColor : DesignSystem.BorderColor;
        button.BackColor = primary ? DesignSystem.PrimaryColor : DesignSystem.SecondaryButtonColor;
        button.ForeColor = primary ? Color.White : DesignSystem.TextBody;
        button.Font = DesignSystem.FontBody;
        button.Cursor = Cursors.Hand;
        button.Margin = new Padding(0, 0, 8, 8);
        button.UseVisualStyleBackColor = false;
        button.MouseEnter += (_, _) =>
        {
            button.BackColor = primary ? DesignSystem.PrimaryHoverColor : DesignSystem.BorderColor;
            button.ForeColor = primary ? Color.White : DesignSystem.TextTitle;
        };
        button.MouseLeave += (_, _) =>
        {
            button.BackColor = primary ? DesignSystem.PrimaryColor : DesignSystem.SecondaryButtonColor;
            button.ForeColor = primary ? Color.White : DesignSystem.TextBody;
        };
    }

    private static void StyleInput(Control control)
    {
        control.BackColor = DesignSystem.SurfaceColor;
        control.ForeColor = DesignSystem.TextBody;
        control.Font = DesignSystem.FontBody;
        control.Margin = new Padding(0, 0, 8, 8);
    }

    private static void StyleOption(ButtonBase control)
    {
        control.BackColor = DesignSystem.SurfaceColor;
        control.ForeColor = DesignSystem.TextBody;
        control.Font = DesignSystem.FontBody;
        control.FlatStyle = FlatStyle.Flat;
        control.FlatAppearance.BorderSize = 0;
        control.Margin = new Padding(0, 0, 14, 8);
        control.UseVisualStyleBackColor = false;
    }

    private void ToggleDetails()
    {
        detailsExpanded = !detailsExpanded;
        detailsPanel.Visible = detailsExpanded;
        detailsToggleButton.Text = detailsExpanded ? "▲ 모니터링 상세 정보 닫기" : "▼ 모니터링 상세 정보 열기";
        AdjustFormHeight();
    }

    private void ToggleSettings()
    {
        settingsExpanded = !settingsExpanded;
        settingsPanel.Visible = settingsExpanded;
        settingsToggleButton.Text = settingsExpanded ? "설정 닫기" : "설정 열기";
        AdjustFormHeight();
    }

    private void AdjustFormHeight()
    {
        var workingArea = Screen.FromControl(this).WorkingArea;
        contentPanel.PerformLayout();
        var desiredFormHeight = contentPanel.PreferredSize.Height + (Height - ClientSize.Height) + (DesignSystem.CardPadding.Vertical * 2);
        if (detailsExpanded)
        {
            desiredFormHeight += DesignSystem.ControlMargin;
        }
        Height = Math.Min(desiredFormHeight, workingArea.Height - 40);
    }

    private void UpdateModeDescription()
    {
        if (dryRunRadio.Checked)
        {
            UpdatePrimaryButtonText();
            modeDescriptionValue.Text = "현재 모드는 관찰 전용입니다. 게임/윈도우 설정은 바꾸지 않고 적용 가능 여부와 위험 신호만 확인합니다.";
            if (runningProcess is null)
            {
                optimizeStateValue.Text = "상태 점검 대기 중";
            }
            return;
        }

        if (softApplyRadio.Checked)
        {
            UpdatePrimaryButtonText();
            modeDescriptionValue.Text = "기본 최적화 모드입니다. 시간 제한 없이 유지되며, 위험 신호가 있으면 적용하지 않거나 되돌릴 수 있게 동작합니다.";
            if (runningProcess is null)
            {
                optimizeStateValue.Text = "최적화 대기 중";
            }
            return;
        }

        UpdatePrimaryButtonText();
        modeDescriptionValue.Text = "현재 모드는 실제 적용입니다. 스레드 우선순위와 affinity 같은 실행 설정을 변경할 수 있습니다.";
        if (runningProcess is null)
        {
            optimizeStateValue.Text = "실제 적용 준비";
        }
    }

    private void UpdateRuntimeLimitState()
    {
        runtimeSecondsBox.Enabled = runtimeLimitCheck.Checked;
        runtimeDescriptionValue.Text = runtimeLimitCheck.Checked
            ? "지정한 시간이 지나면 엔진이 종료되고 복구 절차가 진행됩니다."
            : "시간 제한 없이 유지됩니다. 사용자가 종료를 누르거나 엔진이 종료될 때까지 계속 동작합니다.";
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
        else
        {
            targetCombo.SelectedIndex = -1;
            targetCombo.Text = string.Empty;
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
            MessageBox.Show("대상 프로세스를 선택하거나 입력하세요.", "대상 필요", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var enginePath = FindEnginePath();
        if (enginePath is null)
        {
            MessageBox.Show("GameOptimizer.exe를 찾지 못했습니다. Release 빌드를 먼저 생성하세요.", "엔진 없음", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        ClearLog();
        UpdateSummaryState("세팅중", DesignSystem.PrimaryColor, "최적화 준비 중");
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
        process.Exited += (_, _) => BeginInvoke(new Action(() =>
        {
            var exitCode = process.ExitCode;
            process.Dispose();
            runningProcess = null;
            UpdateControlState(false);
            UpdateSummaryState("대기", exitCode == 0 ? DesignSystem.Success : DesignSystem.Danger, exitCode == 0 ? "최적화 완료" : "최적화 중단");
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
            UpdateSummaryState("적용중", DesignSystem.Success, "최적화 적용 중");
            AppendLogLine($"[INFO] UI: {enginePath.FullName}");
            AppendLogLine($"[INFO] UI: GameOptimizer.exe {FormatArgumentsForLog(arguments)}");
            await Task.CompletedTask;
        }
        catch (Exception ex)
        {
            runningProcess = null;
            process.Dispose();
            UpdateControlState(false);
            UpdateSummaryState("대기", DesignSystem.Danger, "최적화 실패");
            AppendLogLine($"[BLOCKER] UI: 실행 실패 - {ex.Message}");
        }
    }

    private List<string> BuildArgumentList(TargetSelection target)
    {
        var args = new List<string> { target.ExeName };
        if (target.ProcessId is { } processId)
        {
            args.Add("--pid");
            args.Add(processId.ToString());
        }
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
            args.Add(latencyPingText.Text.Trim());
        }
        if (backgroundFilterCheck.Checked && !string.IsNullOrWhiteSpace(backgroundFilterText.Text))
        {
            args.Add("--background-filter");
            args.Add(backgroundFilterText.Text.Trim());
        }
        if (runtimeLimitCheck.Checked)
        {
            args.Add("--max-runtime-seconds");
            args.Add(((int)runtimeSecondsBox.Value).ToString());
        }
        return args;
    }

    private string GetSelectedTargetName()
    {
        if (targetCombo.SelectedItem is ProcessListItem item)
        {
            return item.ExeName;
        }
        return targetCombo.Text.Trim();
    }

    private TargetSelection GetSelectedTarget()
    {
        if (targetCombo.SelectedItem is ProcessListItem item)
        {
            return new TargetSelection { ExeName = item.ExeName, ProcessId = item.ProcessId };
        }

        return new TargetSelection { ExeName = targetCombo.Text.Trim() };
    }

    private static string FormatArgumentsForLog(IEnumerable<string> arguments)
    {
        return string.Join(" ", arguments.Select(Quote));
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private void StopEngine()
    {
        if (runningProcess is null)
        {
            UpdateSummaryState("대기", DesignSystem.Success, "종료 완료");
            AppendLogLine("[INFO] UI: 실행 중인 엔진이 없어 상태만 복구 완료로 표시했습니다.");
            return;
        }

        try
        {
            runningProcess.Kill(entireProcessTree: true);
            UpdateSummaryState("종료중", DesignSystem.Warning, "종료 실행 중");
            AppendLogLine("[WARN] UI: 사용자가 종료를 요청했습니다.");
        }
        catch (Exception ex)
        {
            AppendLogLine($"[WARN] UI: 종료 실패 - {ex.Message}");
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

        if (line.Contains("[BLOCKER]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[ERROR]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase))
        {
            statusValue.ForeColor = DesignSystem.Danger;
            optimizeStateValue.Text = "최적화 중단";
        }
        else if (line.Contains("[WARN]", StringComparison.OrdinalIgnoreCase))
        {
            if (statusValue.ForeColor != DesignSystem.Danger)
            {
                statusValue.ForeColor = DesignSystem.Warning;
            }
        }

        hiddenLogLines.Add(line);
    }

    private void ClearLog()
    {
        hiddenLogLines.Clear();
    }

    private void UpdateSummaryState(string state, Color color, string optimizeText)
    {
        statusValue.Text = $"현재 상태 : {state}";
        statusValue.Font = DesignSystem.FontHeading;
        statusValue.ForeColor = color;
        optimizeStateValue.Text = optimizeText;
        recoveryStateValue.Text = "자동 복구 가능";
    }

    private void UpdateControlState(bool running)
    {
        startButton.Enabled = true;
        UpdatePrimaryButtonText(running);
        targetCombo.Enabled = !running;
        refreshButton.Enabled = !running;
        dryRunRadio.Enabled = !running;
        softApplyRadio.Enabled = !running;
        applyRadio.Enabled = !running;
    }

    private void UpdatePrimaryButtonText(bool? runningOverride = null)
    {
        var running = runningOverride ?? runningProcess is not null;
        startButton.Text = running ? "[종료]" : "[최적화]";
    }

    private void UpdateEnginePathLabel()
    {
        var path = FindEnginePath();
        enginePathValue.Text = path is null ? "엔진: 찾을 수 없음" : $"엔진: {path.FullName}";
        enginePathValue.ForeColor = path is null ? DesignSystem.Danger : DesignSystem.TextMuted;
    }

    private void UpdateApplyWarning()
    {
        UpdateModeDescription();
        if (applyRadio.Checked)
        {
            UpdateSummaryState("대기", DesignSystem.Warning, "실제 적용 준비");
        }
        else if (runningProcess is null)
        {
            statusValue.Text = "현재 상태 : 대기";
            statusValue.ForeColor = DesignSystem.TextMuted;
            recoveryStateValue.Text = "자동 복구 가능";
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

        var selectedTarget = GetSelectedTargetName();
        var reports = evidence.GetFiles("rc_evidence_report.txt", SearchOption.AllDirectories)
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .ToList();
        var report = string.IsNullOrWhiteSpace(selectedTarget)
            ? reports.FirstOrDefault()
            : reports.FirstOrDefault(file => ReportMatchesTarget(file, selectedTarget));
        if (report is null)
        {
            MessageBox.Show("선택한 대상의 rc_evidence_report.txt가 없습니다.", "리포트 없음", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
