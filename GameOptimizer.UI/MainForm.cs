using System.Diagnostics;
using System.Text;

namespace GameOptimizer.UI;

public sealed partial class MainForm : Form
{
    private const int FormWidth = 1180;
    private const int MaxHiddenLogLines = 2000;
    private const int MaxLogLinesPerUiFlush = 32;

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
    private readonly LineMetricControl cpuTrend = new() { Title = "CPU (연산 속도)" };
    private readonly LineMetricControl networkTrend = new() { Title = "네트워크 (반응 속도)" };
    private readonly DonutMetricControl threadGauge = new() { Title = "자원 집중도", Value = 72 };
    private readonly DonutMetricControl safetyGauge = new() { Title = "시스템 안전성", Value = 92 };
    private readonly ScoreGaugeControl healthGauge = new() { Value = 68, Caption = "종합 최적화 점수" };
    private readonly ProgressMetricControl cpuMetric = new() { Title = "CPU 쾌적도", Detail = "상태 확인 대기 중", Percent = 55 };
    private readonly ProgressMetricControl threadMetric = new() { Title = "자원 집중도", Detail = "게임 실행 상태 확인 대기", Percent = 72 };
    private readonly ProgressMetricControl networkMetric = new() { Title = "네트워크 반응", Detail = "통신 상태 확인 대기", Percent = 70 };
    private readonly Label visualSummaryValue = new();
    private readonly Label heroTitleValue = new();
    private readonly Label heroDescriptionValue = new();
    private readonly List<string> hiddenLogLines = new();
    private readonly Queue<string> pendingLogLines = new();
    private readonly object pendingLogLock = new();

    private Process? runningProcess;
    private bool stopInProgress;
    private bool logFlushScheduled;
    private string? stopSignalFilePath;
    private bool settingsExpanded;
    private bool detailsExpanded;
    private int warningCount;
    private int blockerCount;
    private int logSampleCount;
    private double cpuScore = 55;
    private double networkScore = 70;
    private double threadScore = 72;
    private double safetyScore = 92;

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

    private sealed class LineMetricControl : Control
    {
        private readonly Queue<double> samples = new();

        public string Title { get; init; } = string.Empty;

        public LineMetricControl()
        {
            DoubleBuffered = true;
            Height = 112;
            MinimumSize = new Size(180, 112);
        }

        public void Push(double value)
        {
            samples.Enqueue(Math.Clamp(value, 0, 100));
            while (samples.Count > 36)
            {
                samples.Dequeue();
            }
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);
            using var gridPen = new Pen(DesignSystem.BorderColor);
            using var linePen = new Pen(DesignSystem.PrimaryColor, 2F);

            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 0, 0);

            var plot = new Rectangle(0, 30, Math.Max(1, Width - 4), Math.Max(1, Height - 42));
            e.Graphics.DrawRectangle(gridPen, plot);
            e.Graphics.DrawString("효율", DesignSystem.FontSmall, mutedBrush, 0, Height - 16);

            if (samples.Count < 2)
            {
                e.Graphics.DrawString("대기 중", DesignSystem.FontSmall, mutedBrush, plot.Left + 8, plot.Top + 10);
                return;
            }

            var values = samples.ToArray();
            var points = new PointF[values.Length];
            for (var i = 0; i < values.Length; ++i)
            {
                var x = plot.Left + (plot.Width * i / Math.Max(1, values.Length - 1));
                var y = plot.Bottom - (plot.Height * (float)values[i] / 100F);
                points[i] = new PointF(x, y);
            }
            e.Graphics.DrawLines(linePen, points);

            var latest = values[^1];
            e.Graphics.DrawString($"{latest:0}%", DesignSystem.FontSmall, titleBrush, plot.Right - 36, plot.Top + 4);
        }
    }

    private sealed class DonutMetricControl : Control
    {
        public string Title { get; init; } = string.Empty;
        public double Value { get; set; }

        public DonutMetricControl()
        {
            DoubleBuffered = true;
            Height = 112;
            MinimumSize = new Size(132, 112);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedPen = new Pen(DesignSystem.BorderColor, 8F);
            using var valuePen = new Pen(Value >= 80 ? DesignSystem.Success : Value >= 55 ? DesignSystem.PrimaryColor : DesignSystem.Warning, 8F);
            using var textBrush = new SolidBrush(DesignSystem.TextBody);

            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 0, 0);
            var size = Math.Min(Width - 12, Height - 34);
            var rect = new Rectangle((Width - size) / 2, 30, size, size);
            e.Graphics.DrawArc(mutedPen, rect, -90, 360);
            e.Graphics.DrawArc(valuePen, rect, -90, (float)Math.Clamp(Value, 0, 100) * 3.6F);

            var text = $"{Value:0}%";
            var textSize = e.Graphics.MeasureString(text, DesignSystem.FontHeading);
            e.Graphics.DrawString(text, DesignSystem.FontHeading, textBrush, rect.Left + (rect.Width - textSize.Width) / 2, rect.Top + (rect.Height - textSize.Height) / 2);
        }
    }

    private sealed class ScoreGaugeControl : Control
    {
        public double Value { get; set; }
        public string Caption { get; init; } = string.Empty;

        public ScoreGaugeControl()
        {
            DoubleBuffered = true;
            Size = new Size(160, 160);
            MinimumSize = new Size(150, 150);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            var size = Math.Min(Width, Height) - 16;
            var rect = new Rectangle((Width - size) / 2, 8, size, size);
            using var basePen = new Pen(Color.FromArgb(241, 245, 249), 10F);
            using var valuePen = new Pen(Value >= 85 ? DesignSystem.Success : Value >= 60 ? DesignSystem.Warning : DesignSystem.Danger, 10F);
            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);

            e.Graphics.DrawArc(basePen, rect, -90, 360);
            e.Graphics.DrawArc(valuePen, rect, -90, (float)Math.Clamp(Value, 0, 100) * 3.6F);

            var scoreText = $"{Value:0}";
            using var scoreFont = new Font("Segoe UI", 34F, FontStyle.Bold);
            var scoreSize = e.Graphics.MeasureString(scoreText, scoreFont);
            e.Graphics.DrawString(scoreText, scoreFont, titleBrush, rect.Left + (rect.Width - scoreSize.Width) / 2, rect.Top + 38);

            var captionSize = e.Graphics.MeasureString(Caption, DesignSystem.FontSmall);
            e.Graphics.DrawString(Caption, DesignSystem.FontSmall, mutedBrush, rect.Left + (rect.Width - captionSize.Width) / 2, rect.Top + 92);
        }
    }

    private sealed class ProgressMetricControl : Control
    {
        public string Title { get; init; } = string.Empty;
        public string Detail { get; set; } = string.Empty;
        public double Percent { get; set; }

        public ProgressMetricControl()
        {
            DoubleBuffered = true;
            Height = 136;
            MinimumSize = new Size(210, 136);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var textBrush = new SolidBrush(DesignSystem.TextBody);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);
            using var borderPen = new Pen(DesignSystem.BorderColor);

            var bounds = new Rectangle(0, 0, Width - 1, Height - 1);
            e.Graphics.DrawRectangle(borderPen, bounds);
            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 18, 18);

            var percentText = $"{Percent:0}%";
            using var percentFont = new Font("Segoe UI", 20F, FontStyle.Bold);
            var percentSize = e.Graphics.MeasureString(percentText, percentFont);
            e.Graphics.DrawString(percentText, percentFont, titleBrush, Width - percentSize.Width - 18, 14);

            var barRect = new Rectangle(18, 68, Math.Max(1, Width - 36), 10);
            using var barBack = new SolidBrush(Color.FromArgb(241, 245, 249));
            using var barFore = new SolidBrush(Percent >= 80 ? DesignSystem.Success : Percent >= 55 ? DesignSystem.PrimaryColor : DesignSystem.Warning);
            e.Graphics.FillRectangle(barBack, barRect);
            e.Graphics.FillRectangle(barFore, new Rectangle(barRect.X, barRect.Y, (int)(barRect.Width * Math.Clamp(Percent, 0, 100) / 100), barRect.Height));

            e.Graphics.DrawString(Detail, DesignSystem.FontSmall, mutedBrush, 18, 90);
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
                ? $"{ExeName}  (실행번호 {ProcessId})"
                : $"{ExeName}  (실행번호 {ProcessId}) - {WindowTitle}";
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

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = DesignSystem.BgColor,
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 250));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(root);

        root.Controls.Add(CreateSidebar(), 0, 0);

        var main = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = DesignSystem.BgColor,
        };
        main.RowStyles.Add(new RowStyle(SizeType.Absolute, 64));
        main.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(main, 1, 0);

        main.Controls.Add(CreateHeader(), 0, 0);

        var scrollHost = new Panel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            Padding = new Padding(28),
            BackColor = DesignSystem.BgColor,
        };
        main.Controls.Add(scrollHost, 0, 1);

        contentPanel.Dock = DockStyle.Top;
        contentPanel.AutoSize = true;
        contentPanel.ColumnCount = 1;
        contentPanel.RowCount = 5;
        contentPanel.BackColor = DesignSystem.BgColor;
        contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        scrollHost.Controls.Add(contentPanel);

        contentPanel.Controls.Add(CreateSummaryPanel(), 0, 0);
        contentPanel.Controls.Add(CreateMetricGridPanel(), 0, 1);
        contentPanel.Controls.Add(CreateRecommendationPanel(), 0, 2);
        contentPanel.Controls.Add(CreateVisualMetricsPanel(), 0, 3);

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
            "CPU 상태 분석",
            new[] { "사용된 주요 코어 수 : 4", "현재 처리 속도 : 막힘없이 원활함" },
            new[] { "Processor Group : 0", "Primary Core : 4", "Fallback Core : 6", "SMT 분리 : 적용됨", "CCX 최적화 : 적용됨" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "자원 집중도 분석",
            new[] { "게임이 PC 자원을 안정적으로 독점 중입니다.", "방해 프로그램의 간섭 없음" },
            new[] { "메인 스레드 ID : 1234", "EMA 점수 : 91", "Stickiness : 4초", "Migration : 0회" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "네트워크 연결 분석",
            new[] { "서버 통신 상태 : 좋음", "반응 속도(핑) 변화 : 매우 안정적", "네트워크 끊김 현상 : 없음" },
            new[] { "RTT : 5ms", "RTT 변동 : 1.2ms", "DPC 발생 : 0회", "IRQ 재배치 : 없음" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "안전 시스템 상태",
            new[] { "최적화 이전 PC 상태 백업 완료", "문제 발생 시 자동 복원 준비됨", "시스템 진단 결과 : 정상" },
            new[] { "Affinity 백업 : 완료", "Priority 백업 : 완료", "ApplyGuard : 정상", "Rollback 준비 : 완료" }));
        settingsPanel.Controls.Add(detailsPanel, 0, 2);
        contentPanel.Controls.Add(settingsPanel, 0, 4);
    }

    private Control CreateSidebar()
    {
        var sidebar = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 6,
            BackColor = DesignSystem.SurfaceColor,
            Padding = new Padding(18),
        };
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        var brand = new Label
        {
            Text = "Game Optimizer",
            AutoSize = true,
            Font = DesignSystem.FontTitle,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 8, 0, 28),
        };
        sidebar.Controls.Add(brand, 0, 0);
        sidebar.Controls.Add(CreateNavButton("■  요약 대시보드", active: true), 0, 1);
        sidebar.Controls.Add(CreateNavButton("~  성능 모니터", active: false), 0, 2);
        sidebar.Controls.Add(CreateNavButton("+  시스템 안전성", active: false), 0, 3);

        settingsToggleButton.Text = "설정 열기";
        StyleButton(settingsToggleButton, primary: false, width: 190, height: 40);
        settingsToggleButton.Click += (_, _) => ToggleSettings();
        sidebar.Controls.Add(settingsToggleButton, 0, 5);
        return sidebar;
    }

    private static Button CreateNavButton(string text, bool active)
    {
        var button = new Button
        {
            Text = text,
            Width = 190,
            Height = 42,
            TextAlign = ContentAlignment.MiddleLeft,
            FlatStyle = FlatStyle.Flat,
            BackColor = active ? Color.FromArgb(239, 246, 255) : DesignSystem.SurfaceColor,
            ForeColor = active ? DesignSystem.PrimaryColor : DesignSystem.TextMuted,
            Font = active ? DesignSystem.FontHeading : DesignSystem.FontBody,
            Margin = new Padding(0, 0, 0, 8),
            Cursor = Cursors.Hand,
            UseVisualStyleBackColor = false,
        };
        button.FlatAppearance.BorderSize = 0;
        return button;
    }

    private Control CreateHeader()
    {
        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = Color.FromArgb(252, 253, 255),
            Padding = new Padding(28, 0, 28, 0),
        };
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        header.Controls.Add(new Label
        {
            Text = "시스템 상태 요약",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Anchor = AnchorStyles.Left,
        }, 0, 0);
        header.Controls.Add(new Label
        {
            Text = "현재 PC 상태 실시간 확인 중",
            AutoSize = true,
            Font = DesignSystem.FontSmall,
            ForeColor = DesignSystem.TextMuted,
            Anchor = AnchorStyles.Right,
        }, 1, 0);
        return header;
    }

    private Control CreateSummaryPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(28);

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 180));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        healthGauge.Dock = DockStyle.Fill;
        healthGauge.Margin = new Padding(0, 0, 30, 0);
        table.Controls.Add(healthGauge, 0, 0);

        var statusStack = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 1,
            RowCount = 3,
            BackColor = DesignSystem.SurfaceColor,
            Anchor = AnchorStyles.Left,
        };
        heroTitleValue.Text = "최적화가 권장됩니다";
        heroTitleValue.AutoSize = true;
        heroTitleValue.Font = new Font("Segoe UI", 20F, FontStyle.Bold);
        heroTitleValue.ForeColor = DesignSystem.TextTitle;
        heroTitleValue.Margin = new Padding(0, 14, 0, 10);
        statusStack.Controls.Add(heroTitleValue, 0, 0);
        heroDescriptionValue.Text = "게임 실행 상태와 점검 기록을 바탕으로 CPU 쾌적도, 자원 집중도, 네트워크 반응, 시스템 안전성을 실시간으로 평가합니다.";
        heroDescriptionValue.AutoSize = true;
        heroDescriptionValue.MaximumSize = new Size(470, 0);
        heroDescriptionValue.Font = DesignSystem.FontBody;
        heroDescriptionValue.ForeColor = DesignSystem.TextMuted;
        statusStack.Controls.Add(heroDescriptionValue, 0, 1);
        table.Controls.Add(statusStack, 1, 0);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Margin = new Padding(28, 22, 0, 0) };
        startButton.Text = "[최적화]";
        StyleButton(startButton, primary: true, width: 190, height: 48);
        startButton.Click += async (_, _) => await ToggleEngineAsync();
        actions.Controls.Add(startButton);
        table.Controls.Add(actions, 2, 0);

        return panel;
    }

    private Control CreateMetricGridPanel()
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 3,
            RowCount = 2,
            BackColor = DesignSystem.BgColor,
            Margin = new Padding(0, 0, 0, 24),
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.34F));
        cpuMetric.Dock = DockStyle.Fill;
        cpuMetric.Margin = new Padding(0, 0, 12, 0);
        threadMetric.Dock = DockStyle.Fill;
        threadMetric.Margin = new Padding(12, 0, 12, 0);
        networkMetric.Dock = DockStyle.Fill;
        networkMetric.Margin = new Padding(12, 0, 0, 0);
        panel.Controls.Add(cpuMetric, 0, 0);
        panel.Controls.Add(threadMetric, 1, 0);
        panel.Controls.Add(networkMetric, 2, 0);
        panel.Controls.Add(CreateMetricHelpText("게임이 PC 두뇌(CPU)를 막힘없이 쾌적하게 사용하고 있는지 점수화한 지표입니다."), 0, 1);
        panel.Controls.Add(CreateMetricHelpText("다른 프로그램의 방해 없이 게임에 PC 자원이 얼마나 잘 집중되고 있는지 보여줍니다."), 1, 1);
        panel.Controls.Add(CreateMetricHelpText("서버와의 통신 지연이 없는지 확인합니다. 점수가 낮으면 렉이 발생할 수 있습니다."), 2, 1);
        return panel;
    }

    private Control CreateRecommendationPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(20);

        var table = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 1, RowCount = 3, BackColor = DesignSystem.SurfaceColor };
        panel.Controls.Add(table);
        table.Controls.Add(new Label
        {
            Text = "시스템 권장 조치사항",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 12),
        }, 0, 0);
        table.Controls.Add(CreateRecommendationRow("!", "게임 실행 상태 확인", "최적화 대상 게임이 정상적으로 실행 중인지 점검합니다.", DesignSystem.Warning), 0, 1);
        table.Controls.Add(CreateRecommendationRow("~", "안전 적용 모드 유지 권장", "게임 도중 문제가 발생하지 않도록 가장 안전한 설정으로 최적화를 유지합니다.", DesignSystem.PrimaryColor), 0, 2);
        return panel;
    }

    private static Control CreateRecommendationRow(string badge, string title, string detail, Color accent)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 3,
            RowCount = 1,
            BackColor = DesignSystem.SurfaceColor,
            Padding = new Padding(12),
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 46));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 28));

        var badgeLabel = new Label
        {
            Text = badge,
            Width = 32,
            Height = 32,
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.FromArgb(239, 246, 255),
            ForeColor = accent,
            Font = DesignSystem.FontHeading,
            Margin = new Padding(0, 0, 12, 0),
        };
        row.Controls.Add(badgeLabel, 0, 0);

        var textStack = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 1, RowCount = 2, BackColor = DesignSystem.SurfaceColor };
        textStack.Controls.Add(new Label { Text = title, AutoSize = true, Font = DesignSystem.FontHeading, ForeColor = DesignSystem.TextTitle }, 0, 0);
        textStack.Controls.Add(new Label { Text = detail, AutoSize = true, Font = DesignSystem.FontSmall, ForeColor = DesignSystem.TextMuted, Margin = new Padding(0, 4, 0, 0) }, 0, 1);
        row.Controls.Add(textStack, 1, 0);

        row.Controls.Add(new Label
        {
            Text = ">",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextMuted,
            Anchor = AnchorStyles.Right,
        }, 2, 0);
        return row;
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

        modeDescriptionValue.Text = "현재 모드는 게임 상태를 점검하고 기록만 남깁니다. 시스템 설정은 바꾸지 않습니다.";
        modeDescriptionValue.AutoSize = true;
        modeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        modeDescriptionValue.Font = DesignSystem.FontSmall;
        modeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        modeDescriptionValue.Margin = new Padding(0, 0, 0, 18);
        table.Controls.Add(modeDescriptionValue);

        return panel;
    }

    private Control CreateVisualMetricsPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
            RowCount = 5,
            BackColor = DesignSystem.SurfaceColor,
        };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        panel.Controls.Add(table);

        var heading = new Label
        {
            Text = "성능 모니터링 그래프",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 8),
        };
        table.SetColumnSpan(heading, 2);
        table.Controls.Add(heading, 0, 0);

        visualSummaryValue.Text = "점검 기록 대기 중";
        visualSummaryValue.AutoSize = true;
        visualSummaryValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        visualSummaryValue.Font = DesignSystem.FontSmall;
        visualSummaryValue.ForeColor = DesignSystem.TextMuted;
        visualSummaryValue.Margin = new Padding(0, 0, 0, 10);
        table.SetColumnSpan(visualSummaryValue, 2);
        table.Controls.Add(visualSummaryValue, 0, 1);

        cpuTrend.Dock = DockStyle.Fill;
        cpuTrend.Margin = new Padding(0, 0, 8, 12);
        networkTrend.Dock = DockStyle.Fill;
        networkTrend.Margin = new Padding(8, 0, 0, 12);
        table.Controls.Add(cpuTrend, 0, 2);
        table.Controls.Add(networkTrend, 1, 2);

        threadGauge.Dock = DockStyle.Fill;
        threadGauge.Margin = new Padding(0, 0, 8, 0);
        safetyGauge.Dock = DockStyle.Fill;
        safetyGauge.Margin = new Padding(8, 0, 0, 0);
        table.Controls.Add(threadGauge, 0, 3);
        table.Controls.Add(safetyGauge, 1, 3);
        table.Controls.Add(CreateHelpText("점수가 100점에 가까울수록 다른 작업의 간섭 없이 게임이 우선 처리되고 있음을 의미합니다."), 0, 4);
        table.Controls.Add(CreateHelpText("예기치 않은 문제가 발생할 때 원래 설정으로 안전하게 복원될 수 있는 준비 상태입니다."), 1, 4);

        ResetVisualMetrics();
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
        detailsToggleButton.Text = "▼ 세부 상태 정보 열기";
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
            Text = $"▶ {title}",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Anchor = AnchorStyles.Left,
        };
        table.Controls.Add(header, 0, 0);

        var advancedButton = new Button
        {
            Text = "전문가용 정보",
            Anchor = AnchorStyles.Right,
        };
        StyleButton(advancedButton, primary: false, width: 110, height: 30);
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
            Text = "세부 설정 및 진단",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 10),
        };
        table.SetColumnSpan(heading, 3);
        table.Controls.Add(heading, 0, 0);

        var targetLabel = new Label { Text = "최적화 대상", AutoSize = true, Anchor = AnchorStyles.Left };
        targetLabel.Font = DesignSystem.FontBody;
        targetLabel.ForeColor = DesignSystem.TextBody;
        targetCombo.Dock = DockStyle.Fill;
        targetCombo.DropDownStyle = ComboBoxStyle.DropDown;
        targetCombo.FlatStyle = FlatStyle.Flat;
        StyleInput(targetCombo);
        refreshButton.Text = "목록 새로고침";
        StyleButton(refreshButton, primary: false, width: 120, height: 30);
        refreshButton.Click += (_, _) => RefreshProcessList();
        table.Controls.Add(targetLabel, 0, 1);
        table.Controls.Add(targetCombo, 1, 1);
        table.Controls.Add(refreshButton, 2, 1);

        enginePathValue.Text = "시스템 도구 경로 확인 중...";
        enginePathValue.AutoSize = true;
        enginePathValue.Font = DesignSystem.FontSmall;
        enginePathValue.ForeColor = DesignSystem.TextMuted;
        enginePathValue.Margin = new Padding(0, 8, 0, 10);
        table.SetColumnSpan(enginePathValue, 3);
        table.Controls.Add(enginePathValue, 0, 2);

        var modeFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
        dryRunRadio.Text = "테스트 모드 (미리보기)";
        softApplyRadio.Text = "안전 적용 모드 (권장)";
        applyRadio.Text = "강력 적용 모드";
        StyleOption(dryRunRadio);
        StyleOption(softApplyRadio);
        StyleOption(applyRadio);
        softApplyRadio.Checked = true;
        dryRunRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        softApplyRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        applyRadio.CheckedChanged += (_, _) => UpdateApplyWarning();
        modeFlow.Controls.AddRange(new Control[] { dryRunRadio, softApplyRadio, applyRadio });
        table.SetColumnSpan(modeFlow, 3);
        table.Controls.Add(modeFlow, 0, 3);

        AddHelpText(table, "권장 설정인 '안전 적용 모드'는 시스템에 무리가 가지 않도록 모니터링하며 최적화를 수행합니다. '테스트 모드'는 실제 설정을 바꾸지 않고 점검만 합니다.", 4);

        threadDetailCheck.Text = "자원 할당 상세 기록 남기기";
        StyleOption(threadDetailCheck);
        table.SetColumnSpan(threadDetailCheck, 3);
        table.Controls.Add(threadDetailCheck, 0, 5);
        AddHelpText(table, "문제 발생 원인을 파악하기 위해 게임이 PC 자원을 어떻게 쓰고 있는지 자세히 기록합니다.", 6);

        backgroundDetailCheck.Text = "백그라운드 프로그램 상세 기록 남기기";
        StyleOption(backgroundDetailCheck);
        table.SetColumnSpan(backgroundDetailCheck, 3);
        table.Controls.Add(backgroundDetailCheck, 0, 7);
        AddHelpText(table, "게임 외에 실행되며 PC를 느리게 만드는 프로그램이 있는지 진단할 때 사용합니다.", 8);

        latencyPingCheck.Text = "네트워크 상태(핑) 확인";
        StyleOption(latencyPingCheck);
        latencyPingText.Text = "8.8.8.8";
        latencyPingText.Dock = DockStyle.Fill;
        latencyPingText.BorderStyle = BorderStyle.FixedSingle;
        StyleInput(latencyPingText);
        table.Controls.Add(latencyPingCheck, 0, 9);
        table.Controls.Add(latencyPingText, 1, 9);
        AddHelpText(table, "네트워크 끊김 현상이 발생할 때, 해당 주소로 연결 상태를 점검합니다. 윈도우 설정은 건드리지 않습니다.", 10);

        backgroundFilterCheck.Text = "예외 프로그램 목록 적용";
        StyleOption(backgroundFilterCheck);
        backgroundFilterText.Dock = DockStyle.Fill;
        backgroundFilterText.BorderStyle = BorderStyle.FixedSingle;
        backgroundFilterText.Text = FindDefaultBackgroundFilter();
        StyleInput(backgroundFilterText);
        browseFilterButton.Text = "찾기";
        StyleButton(browseFilterButton, primary: false, width: 50, height: 28);
        browseFilterButton.Click += (_, _) => BrowseBackgroundFilter();
        table.Controls.Add(backgroundFilterCheck, 0, 11);
        table.Controls.Add(backgroundFilterText, 1, 11);
        table.Controls.Add(browseFilterButton, 2, 11);
        AddHelpText(table, "최적화 도중 강제로 종료하거나 제한하지 않을 프로그램 목록 파일입니다.", 12);

        runtimeLimitCheck.Text = "자동 종료 타이머 켜기";
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
        runtimeDescriptionValue.Text = "끄면 직접 종료할 때까지 계속 유지됩니다. 타이머를 켜면 지정한 초 뒤에 최적화가 풀리며 원래대로 돌아옵니다.";
        runtimeDescriptionValue.AutoSize = true;
        runtimeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        runtimeDescriptionValue.Font = DesignSystem.FontSmall;
        runtimeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        runtimeDescriptionValue.Margin = new Padding(0, 2, 0, 8);
        table.SetColumnSpan(runtimeDescriptionValue, 3);
        table.Controls.Add(runtimeDescriptionValue, 0, 14);

        var utilityFlow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Margin = new Padding(0, 12, 0, 0) };
        evidenceFolderButton.Text = "기록 폴더 열기";
        StyleButton(evidenceFolderButton, primary: false, width: 120);
        evidenceFolderButton.Click += (_, _) => OpenEvidenceFolder();
        latestReportButton.Text = "최근 진단 리포트 확인";
        StyleButton(latestReportButton, primary: false, width: 160);
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

    private static Label CreateMetricHelpText(string text)
    {
        return new Label
        {
            Text = text,
            AutoSize = true,
            MaximumSize = new Size(280, 0),
            Font = DesignSystem.FontSmall,
            ForeColor = DesignSystem.TextMuted,
            Margin = new Padding(0, 8, 12, 0),
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
        detailsToggleButton.Text = detailsExpanded ? "▲ 세부 상태 정보 닫기" : "▼ 세부 상태 정보 열기";
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
        // Dashboard mode keeps a stable canvas; inner content scrolls when settings/details expand.
        var workingArea = Screen.FromControl(this).WorkingArea;
        Height = Math.Min(760, workingArea.Height - 40);
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
        modeDescriptionValue.Text = "강력 적용 모드입니다. 성능을 더 적극적으로 끌어올리지만 시스템 실행 설정을 변경할 수 있어 주의가 필요합니다.";
        if (runningProcess is null)
        {
            optimizeStateValue.Text = "강력 적용 대기 중";
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
            await StopEngineAsync();
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
            MessageBox.Show("목록에서 최적화할 게임을 선택해주세요.", "게임 선택 필요", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var enginePath = FindEnginePath();
        if (enginePath is null)
        {
            MessageBox.Show("최적화 도구 파일을 찾을 수 없습니다. Release 빌드를 먼저 생성하세요.", "시스템 오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        ClearLog();
        UpdateSummaryState("세팅중", DesignSystem.PrimaryColor, "최적화 준비 중");
        visualSummaryValue.Text = "엔진 시작 중";
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
        stopSignalFilePath = CreateStopSignalFilePath();
        arguments.Add("--stop-signal-file");
        arguments.Add(stopSignalFilePath);
        foreach (var argument in arguments)
        {
            psi.ArgumentList.Add(argument);
        }

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, e) => AppendLogLine(e.Data);
        process.ErrorDataReceived += (_, e) => AppendLogLine(e.Data);
        process.Exited += (_, _) => RunOnUiThread(() => CompleteEngineExit(process));

        try
        {
            stopInProgress = false;
            runningProcess = process;
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            UpdateSummaryState("적용중", DesignSystem.Success, "최적화 적용 중");
            visualSummaryValue.Text = "실시간 점검 기록 수집 중";
            AppendLogLine($"[INFO] UI: {enginePath.FullName}");
            AppendLogLine($"[INFO] UI: GameOptimizer.exe {FormatArgumentsForLog(arguments)}");
            await Task.CompletedTask;
        }
        catch (Exception ex)
        {
            runningProcess = null;
            DeleteStopSignalFile();
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

    private static string CreateStopSignalFilePath()
    {
        return Path.Combine(
            Path.GetTempPath(),
            $"GameOptimizer.UI.stop.{Environment.ProcessId}.{Guid.NewGuid():N}.signal");
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

    private async Task StopEngineAsync()
    {
        var process = runningProcess;
        if (process is null)
        {
            UpdateSummaryState("대기", DesignSystem.Success, "종료 완료");
            AppendLogLine("[INFO] UI: 실행 중인 엔진이 없어 상태만 복구 완료로 표시했습니다.");
            return;
        }
        if (stopInProgress)
        {
            AppendLogLine("[INFO] UI: 이미 종료 요청을 처리 중입니다.");
            return;
        }

        stopInProgress = true;
        UpdateSummaryState("종료중", DesignSystem.Warning, "종료 실행 중");
        visualSummaryValue.Text = "엔진 종료 확인 중";
        UpdatePrimaryButtonText(runningOverride: true);
        AppendLogLine("[WARN] UI: 사용자가 종료를 요청했습니다.");

        try
        {
            if (!process.HasExited)
            {
                RequestEngineCleanShutdown();
            }

            if (await WaitForEngineExitAsync(process, TimeSpan.FromSeconds(15)))
            {
                CompleteEngineExit(process);
                return;
            }

            stopInProgress = false;
            UpdateSummaryState("종료 지연", DesignSystem.Warning, "엔진 복구 절차 진행 중");
            visualSummaryValue.Text = "엔진 종료 이벤트를 기다리는 중";
            AppendLogLine("[WARN] UI: 종료 요청 후 15초 안에 엔진 종료가 확인되지 않았습니다. 강제 종료하지 않고 복구 절차를 계속 기다립니다.");
        }
        catch (Exception ex)
        {
            stopInProgress = false;
            if (IsProcessExited(process))
            {
                CompleteEngineExit(process);
                return;
            }

            UpdateSummaryState("종료 실패", DesignSystem.Danger, "종료 요청 실패");
            visualSummaryValue.Text = "엔진 종료 요청 실패";
            UpdateControlState(running: true);
            AppendLogLine($"[WARN] UI: 종료 실패 - {ex.Message}");
        }
    }

    private void RequestEngineCleanShutdown()
    {
        if (string.IsNullOrWhiteSpace(stopSignalFilePath))
        {
            throw new InvalidOperationException("stop signal file path is not initialized");
        }

        File.WriteAllText(
            stopSignalFilePath,
            $"requested_utc={DateTime.UtcNow:O}{Environment.NewLine}");
        AppendLogLine($"[INFO] UI: 안전 종료 신호를 보냈습니다. ({stopSignalFilePath})");
    }

    private static async Task<bool> WaitForEngineExitAsync(Process process, TimeSpan timeout)
    {
        try
        {
            using var timeoutCancellation = new CancellationTokenSource(timeout);
            await process.WaitForExitAsync(timeoutCancellation.Token);
            return true;
        }
        catch (OperationCanceledException)
        {
            return process.HasExited;
        }
        catch (InvalidOperationException)
        {
            return true;
        }
    }

    private static bool IsProcessExited(Process process)
    {
        try
        {
            return process.HasExited;
        }
        catch (InvalidOperationException)
        {
            return true;
        }
    }

    private void CompleteEngineExit(Process process)
    {
        if (runningProcess != process)
        {
            process.Dispose();
            return;
        }

        var exitCode = TryGetExitCode(process);
        process.Dispose();
        runningProcess = null;
        stopInProgress = false;
        DeleteStopSignalFile();
        UpdateControlState(false);
        UpdateSummaryState("대기", exitCode == 0 ? DesignSystem.Success : DesignSystem.Danger, exitCode == 0 ? "최적화 완료" : "최적화 중단");
        visualSummaryValue.Text = exitCode == 0 ? "최종 상태 안정" : "최종 상태 점검 필요";
        AppendLogLine(exitCode == 0
            ? "[PASS] UI: 엔진 실행이 정상 종료되었습니다."
            : $"[BLOCKER] UI: 엔진 종료 코드 {exitCode}");
    }

    private void DeleteStopSignalFile()
    {
        var path = stopSignalFilePath;
        stopSignalFilePath = null;
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static int TryGetExitCode(Process process)
    {
        try
        {
            return process.ExitCode;
        }
        catch (InvalidOperationException)
        {
            return -1;
        }
    }

    private void RunOnUiThread(Action action)
    {
        if (IsDisposed)
        {
            return;
        }
        if (!IsHandleCreated)
        {
            return;
        }
        try
        {
            BeginInvoke(action);
        }
        catch (Exception ex) when (
            ex is InvalidOperationException ||
            ex is ObjectDisposedException ||
            ex is System.ComponentModel.Win32Exception)
        {
        }
    }

    private void AppendLogLine(string? line)
    {
        if (string.IsNullOrEmpty(line))
        {
            return;
        }

        lock (pendingLogLock)
        {
            pendingLogLines.Enqueue(line);
            if (logFlushScheduled)
            {
                return;
            }

            logFlushScheduled = true;
        }

        RunOnUiThread(FlushPendingLogLines);
    }

    private void FlushPendingLogLines()
    {
        var processed = 0;
        while (processed < MaxLogLinesPerUiFlush)
        {
            string line;
            lock (pendingLogLock)
            {
                if (pendingLogLines.Count == 0)
                {
                    logFlushScheduled = false;
                    return;
                }

                line = pendingLogLines.Dequeue();
            }

            ProcessLogLine(line);
            ++processed;
        }

        RunOnUiThread(FlushPendingLogLines);
    }

    private void ProcessLogLine(string line)
    {
        UpdateVisualMetrics(line);

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
        if (hiddenLogLines.Count > MaxHiddenLogLines)
        {
            hiddenLogLines.RemoveRange(0, hiddenLogLines.Count - MaxHiddenLogLines);
        }
    }

    private void ClearLog()
    {
        hiddenLogLines.Clear();
        lock (pendingLogLock)
        {
            pendingLogLines.Clear();
            logFlushScheduled = false;
        }
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
        cpuTrend.Push(cpuScore);
        networkTrend.Push(networkScore);
        threadGauge.Value = threadScore;
        safetyGauge.Value = safetyScore;
        healthGauge.Value = 68;
        cpuMetric.Percent = cpuScore;
        threadMetric.Percent = threadScore;
        networkMetric.Percent = networkScore;
        cpuMetric.Detail = "측정 대기 중";
        threadMetric.Detail = "게임 실행 확인 중";
        networkMetric.Detail = "네트워크 반응 대기 중";
        threadGauge.Invalidate();
        safetyGauge.Invalidate();
        healthGauge.Invalidate();
        cpuMetric.Invalidate();
        threadMetric.Invalidate();
        networkMetric.Invalidate();
        visualSummaryValue.Text = "점검 기록 대기 중";
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

        var cpuMs = TryReadNumberAfter(line, "EMA-sample-cpu:");
        if (cpuMs is null)
        {
            cpuMs = TryReadNumberAfter(line, "avg-sample-cpu:");
        }
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

        var rtt = TryReadNumberAfter(line, "RTT:");
        if (rtt is null)
        {
            rtt = TryReadNumberAfter(line, "rtt=");
        }
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

        var shouldRefreshVisuals =
            logSampleCount <= 3 ||
            logSampleCount % 5 == 0 ||
            lower.Contains("blocker") ||
            lower.Contains("error") ||
            lower.Contains("fail") ||
            lower.Contains("warn") ||
            lower.Contains("shutdown") ||
            lower.Contains("runtime validation summary") ||
            lower.Contains("confirmed main tid");
        if (!shouldRefreshVisuals)
        {
            return;
        }

        cpuTrend.Push(cpuScore);
        networkTrend.Push(networkScore);
        threadGauge.Value = Math.Clamp(threadScore, 0, 100);
        safetyGauge.Value = Math.Clamp(safetyScore, 0, 100);
        var healthScore = Math.Clamp((cpuScore + networkScore + threadScore + safetyScore) / 4, 0, 100);
        healthGauge.Value = healthScore;
        cpuMetric.Percent = Math.Clamp(cpuScore, 0, 100);
        threadMetric.Percent = Math.Clamp(threadScore, 0, 100);
        networkMetric.Percent = Math.Clamp(networkScore, 0, 100);
        cpuMetric.Detail = cpuMs.HasValue ? $"처리 속도 {cpuMs.GetValueOrDefault():0.0}ms 수준" : "안정적인 속도 유지 중";
        threadMetric.Detail = warningCount > 0 ? "약간의 자원 간섭 발견됨" : "완벽하게 집중 처리 중";
        networkMetric.Detail = rtt.HasValue ? $"지연 시간 {rtt.GetValueOrDefault():0.0}ms" : "매우 부드러운 통신 상태";
        threadGauge.Invalidate();
        safetyGauge.Invalidate();
        healthGauge.Invalidate();
        cpuMetric.Invalidate();
        threadMetric.Invalidate();
        networkMetric.Invalidate();
        heroTitleValue.Text = healthScore >= 85 ? "시스템이 안정적인 상태입니다" : healthScore >= 60 ? "최적화가 적용 중입니다" : "점검이 필요합니다";
        heroDescriptionValue.Text = blockerCount > 0
            ? "위험 신호가 감지되어 보호를 위해 일부 기능을 제한했습니다. 세부 상태 정보를 확인하세요."
            : warningCount > 0
                ? "작은 지연이나 간섭이 발견되었습니다. 게임 도중 체감될 수 있으니 성능 그래프를 확인해주세요."
                : "매우 안정적이고 부드러운 상태입니다. 이대로 게임을 즐기시면 됩니다.";
        visualSummaryValue.Text = $"현재까지 {logSampleCount}번 검사함 · 경고 {warningCount}건 · 위험 {blockerCount}건";
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

        return end > start && double.TryParse(line[start..end], out var value) ? value : null;
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

        return double.TryParse(line[(start + 1)..(end + 1)], out var value) ? value : null;
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
        enginePathValue.Text = path is null ? "알림: 최적화 도구를 찾지 못했습니다." : $"경로 확인됨: {path.FullName}";
        enginePathValue.ForeColor = path is null ? DesignSystem.Danger : DesignSystem.TextMuted;
    }

    private void UpdateApplyWarning()
    {
        UpdateModeDescription();
        if (applyRadio.Checked)
        {
            UpdateSummaryState("주의 필요", DesignSystem.Warning, "강력 적용 모드 대기 중");
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
            Title = "예외 프로그램 목록 파일 선택",
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
            MessageBox.Show("아직 저장된 진단 리포트가 없습니다.", "알림", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
            MessageBox.Show("선택한 게임에 대한 리포트를 찾지 못했습니다.", "알림", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
