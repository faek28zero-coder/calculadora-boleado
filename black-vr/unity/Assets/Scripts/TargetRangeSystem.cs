// ============================================================
//  BLACK VR — TargetRangeSystem
//  Gerencia a fila de alvos: quantidade simultânea, ritmo e
//  dificuldade progressiva ao longo do round.
// ============================================================
using System.Collections.Generic;
using UnityEngine;

namespace BlackVR
{
    public class TargetRangeSystem : MonoBehaviour
    {
        [Header("Alvos")]
        [SerializeField] private Target[] targets;
        [SerializeField] private int minActive = 3;
        [SerializeField] private int maxActive = 5;
        [SerializeField] private float spawnIntervalStart = 1.15f;
        [SerializeField] private float spawnIntervalEnd = 0.32f;

        [Header("Dificuldade")]
        [Tooltip("Curva de progressão usada pelo GameManagerVR (0 = início, 1 = fim do round).")]
        [SerializeField, Range(0f, 1f)] private float progress = 0f;

        private readonly List<Target> pool = new List<Target>();
        private float spawnTimer;

        public float Progress
        {
            get => progress;
            set => progress = Mathf.Clamp01(value);
        }

        public int ActiveCount
        {
            get
            {
                int n = 0;
                foreach (var t in pool)
                    if (t != null && t.IsHittable) n++;
                return n;
            }
        }

        private void Awake()
        {
            if (targets == null || targets.Length == 0)
                targets = GetComponentsInChildren<Target>(true);

            pool.AddRange(targets);
            foreach (var t in pool)
            {
                t.OnTargetDown += HandleTargetDown;
                t.ResetTarget();
            }
        }

        private void OnDestroy()
        {
            foreach (var t in pool)
                if (t != null) t.OnTargetDown -= HandleTargetDown;
        }

        public void StartRange()
        {
            foreach (var t in pool) if (t != null) t.ResetTarget();
            spawnTimer = 0.4f;
            enabled = true;
        }

        public void StopRange()
        {
            enabled = false;
        }

        private void Update()
        {
            spawnTimer -= Time.deltaTime;

            int maxNow = Mathf.RoundToInt(Mathf.Lerp(minActive, maxActive, progress));
            if (spawnTimer <= 0f && ActiveCount < maxNow)
            {
                SpawnOne();
                float interval = Mathf.Lerp(spawnIntervalStart, spawnIntervalEnd, progress);
                spawnTimer = interval * Random.Range(0.85f, 1.15f);
            }
        }

        private void SpawnOne()
        {
            var candidates = new List<Target>();
            foreach (var t in pool)
                if (t != null && !t.IsHittable && t.Current == Target.State.Down) candidates.Add(t);

            if (candidates.Count == 0) return;
            candidates[Random.Range(0, candidates.Count)].Rise();
        }

        private void HandleTargetDown(Target target, int points, bool critical)
        {
            // O GameManagerVR escuta este evento pela própria assinatura pública.
            TargetDown?.Invoke(target, points, critical);
        }

        /// <summary>(alvo, pontos base, crítico)</summary>
        public event System.Action<Target, int, bool> TargetDown;
    }
}
