// ============================================================
//  BLACK VR — GameManagerVR
//  Round de 75 s, pontuação com combo, pausa e resultados.
//  Funciona como "cérebro" da cena de treino.
// ============================================================
using TMPro;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.SceneManagement;

namespace BlackVR
{
    public class GameManagerVR : MonoBehaviour
    {
        [Header("Round")]
        [SerializeField] private float roundSeconds = 75f;
        [SerializeField] private float comboWindow = 3f;
        [SerializeField] private int maxMultiplier = 5;

        [Header("Referências")]
        [SerializeField] private TargetRangeSystem range;
        [SerializeField] private VRPlayerRig player;
        [SerializeField] private XRWeapon[] weapons;
        [SerializeField] private Transform startPoint;

        [Header("UI")]
        [SerializeField] private TMP_Text scoreLabel;
        [SerializeField] private TMP_Text timeLabel;
        [SerializeField] private TMP_Text ammoLabel;
        [SerializeField] private TMP_Text comboLabel;
        [SerializeField] private TMP_Text accuracyLabel;
        [SerializeField] private TMP_Text hitsLabel;
        [SerializeField] private GameObject menuPanel;
        [SerializeField] private GameObject hudPanel;
        [SerializeField] private GameObject resultsPanel;
        [SerializeField] private TMP_Text resultScore;
        [SerializeField] private TMP_Text resultHits;
        [SerializeField] private TMP_Text resultAccuracy;
        [SerializeField] private TMP_Text resultCombo;

        [Header("Eventos")]
        [SerializeField] private UnityEvent onRoundStart;
        [SerializeField] private UnityEvent onRoundEnd;

        public bool IsPlaying { get; private set; }
        public float TimeLeft { get; private set; }
        public int Score { get; private set; }
        public int Hits { get; private set; }
        public int Shots { get; private set; }
        public int Streak { get; private set; }
        public int BestStreak { get; private set; }

        private float streakTimer;
        private bool paused;

        private void Awake()
        {
            if (range == null) range = FindFirstObjectByType<TargetRangeSystem>();
            if (player == null) player = FindFirstObjectByType<VRPlayerRig>();
            if (weapons == null || weapons.Length == 0) weapons = FindObjectsByType<XRWeapon>(FindObjectsSortMode.None);
        }

        private void OnEnable()
        {
            if (range != null) range.TargetDown += HandleTargetDown;
            foreach (var w in weapons)
            {
                if (w == null) continue;
                w.OnHit += HandleWeaponHit;
                w.OnShot += HandleShot;
                w.OnAmmoChanged += HandleAmmoChanged;
            }
        }

        private void OnDisable()
        {
            if (range != null) range.TargetDown -= HandleTargetDown;
            foreach (var w in weapons)
            {
                if (w == null) continue;
                w.OnHit -= HandleWeaponHit;
                w.OnShot -= HandleShot;
                w.OnAmmoChanged -= HandleAmmoChanged;
            }
        }

        private void Start()
        {
            ShowMenu();
        }

        private void Update()
        {
            if (!IsPlaying) return;

            TimeLeft -= Time.deltaTime;
            if (timeLabel != null)
            {
                timeLabel.text = Mathf.CeilToInt(Mathf.Max(0f, TimeLeft)).ToString();
                timeLabel.color = TimeLeft <= 10f ? new Color(0.88f, 0.14f, 0.14f) : Color.white;
            }

            if (range != null)
                range.Progress = 1f - Mathf.Clamp01(TimeLeft / roundSeconds);

            if (streakTimer > 0f)
            {
                streakTimer -= Time.deltaTime;
                if (streakTimer <= 0f)
                {
                    Streak = 0;
                    UpdateComboLabel();
                }
            }

            if (TimeLeft <= 0f) EndRound();
        }

        // ------------------------------------------------------------ fluxo
        public void StartRound()
        {
            Score = 0;
            Hits = 0;
            Shots = 0;
            Streak = 0;
            BestStreak = 0;
            TimeLeft = roundSeconds;
            streakTimer = 0f;
            IsPlaying = true;
            paused = false;

            if (player != null && startPoint != null)
                player.TeleportTo(startPoint.position, startPoint.eulerAngles.y);

            if (range != null) { range.Progress = 0f; range.StartRange(); }

            foreach (var w in weapons)
            {
                if (w == null) continue;
                w.SetRoundActive(true);
            }

            if (menuPanel != null) menuPanel.SetActive(false);
            if (resultsPanel != null) resultsPanel.SetActive(false);
            if (hudPanel != null) hudPanel.SetActive(true);

            RefreshAllLabels();
            onRoundStart?.Invoke();
        }

        public void EndRound()
        {
            IsPlaying = false;
            if (range != null) range.StopRange();
            foreach (var w in weapons) if (w != null) w.SetRoundActive(false);

            if (hudPanel != null) hudPanel.SetActive(false);
            if (resultsPanel != null) resultsPanel.SetActive(true);

            if (resultScore != null) resultScore.text = Score.ToString("N0");
            if (resultHits != null) resultHits.text = Hits.ToString();
            if (resultAccuracy != null) resultAccuracy.text = AccuracyPercent + "%";
            if (resultCombo != null) resultCombo.text = "x" + Multiplier;

            onRoundEnd?.Invoke();
        }

        public void RestartRound() => StartRound();

        public void BackToMenu()
        {
            IsPlaying = false;
            if (range != null) range.StopRange();
            SceneManager.LoadScene(SceneManager.GetActiveScene().buildIndex);
        }

        public void TogglePause()
        {
            if (!IsPlaying) return;
            paused = !paused;
            Time.timeScale = paused ? 0f : 1f;
        }

        private void ShowMenu()
        {
            if (menuPanel != null) menuPanel.SetActive(true);
            if (hudPanel != null) hudPanel.SetActive(false);
            if (resultsPanel != null) resultsPanel.SetActive(false);
            foreach (var w in weapons) if (w != null) w.SetRoundActive(false);
        }

        // ------------------------------------------------------------ pontuação
        private int Multiplier => Mathf.Clamp(1 + Streak / 2, 1, maxMultiplier);

        private int AccuracyPercent => Shots == 0 ? 0 : Mathf.RoundToInt(100f * Hits / Shots);

        private void HandleTargetDown(Target target, int basePoints, bool critical)
        {
            if (!IsPlaying) return;

            Hits++;
            Streak++;
            BestStreak = Mathf.Max(BestStreak, Streak);
            streakTimer = comboWindow;

            int gained = basePoints * Multiplier;
            Score += gained;

            RefreshAllLabels();
            if (critical) Debug.Log($"[BLACK VR] Crítico! +{gained}");
        }

        private void HandleWeaponHit(RaycastHit hit, float damage)
        {
            // tiro que acertou alvo: pontos já vêm do HandleTargetDown
            if (damage > 0f) return;
            // tiro no cenário: perde o combo
            Streak = 0;
            UpdateComboLabel();
        }

        private void HandleShot()
        {
            if (!IsPlaying) return;
            Shots++;
            if (accuracyLabel != null) accuracyLabel.text = AccuracyPercent + "%";
        }

        private void HandleAmmoChanged(int ammo)
        {
            if (ammoLabel == null) return;
            int mag = weapons != null && weapons.Length > 0 && weapons[0] != null ? weapons[0].MagazineSize : 12;
            ammoLabel.text = $"{ammo}<size=60%>/{mag}</size>";
        }

        private void RefreshAllLabels()
        {
            if (scoreLabel != null) scoreLabel.text = Score.ToString("N0");
            if (hitsLabel != null) hitsLabel.text = Hits.ToString();
            if (accuracyLabel != null) accuracyLabel.text = AccuracyPercent + "%";
            UpdateComboLabel();
        }

        private void UpdateComboLabel()
        {
            if (comboLabel == null) return;
            comboLabel.text = Multiplier > 1 ? "COMBO x" + Multiplier : string.Empty;
        }
    }
}
