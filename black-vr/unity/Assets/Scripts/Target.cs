// ============================================================
//  BLACK VR — Target
//  Alvo pop-up com zona crítica central, subida/descida animada
//  e eventos para o placar.
// ============================================================
using System;
using UnityEngine;

namespace BlackVR
{
    public class Target : MonoBehaviour
    {
        public enum State { Down, Rising, Up, Falling, Dead }

        [Header("Movimento")]
        [SerializeField] private float riseTime = 0.22f;
        [SerializeField] private float holdTime = 2.6f;
        [SerializeField] private float fallTime = 0.18f;
        [SerializeField] private float hiddenOffset = 1.35f;
        [Tooltip("Deslocamento local usado como posição escondida (atrás da barreira).")]
        [SerializeField] private Vector3 hiddenDirection = Vector3.down;

        [Header("Pontuação")]
        [SerializeField] private int basePoints = 100;
        [SerializeField] private int criticalPoints = 250;
        [Tooltip("Raio, em metros, da zona crítica ao redor do centro do alvo.")]
        [SerializeField] private float criticalRadius = 0.19f;

        [Header("Feedback")]
        [SerializeField] private GameObject destroyEffect;
        [SerializeField] private AudioClip hitClip;
        [SerializeField] private AudioClip criticalClip;

        public State Current { get; private set; } = State.Down;
        public bool IsHittable => Current == State.Up || Current == State.Rising;
        public int LastPoints { get; private set; }
        public bool LastWasCritical { get; private set; }

        /// <summary>Disparado quando o alvo é atingido: (alvo, pontos, foi crítico).</summary>
        public event Action<Target, int, bool> OnTargetDown;

        private Vector3 upPosition;
        private Vector3 downPosition;
        private float timer;
        private AudioSource audioSource;
        private Collider[] colliders;

        private void Awake()
        {
            upPosition = transform.localPosition;
            downPosition = upPosition + hiddenDirection.normalized * hiddenOffset;
            transform.localPosition = downPosition;

            audioSource = GetComponent<AudioSource>();
            colliders = GetComponentsInChildren<Collider>(true);
        }

        private void Update()
        {
            switch (Current)
            {
                case State.Rising:
                    timer += Time.deltaTime;
                    transform.localPosition = Vector3.Lerp(downPosition, upPosition,
                        EaseOutBack(Mathf.Clamp01(timer / riseTime)));
                    if (timer >= riseTime) { Current = State.Up; timer = 0f; }
                    break;

                case State.Up:
                    timer += Time.deltaTime;
                    if (timer >= holdTime) { Current = State.Falling; timer = 0f; }
                    break;

                case State.Falling:
                    timer += Time.deltaTime;
                    float k = Mathf.Clamp01(timer / fallTime);
                    transform.localPosition = Vector3.Lerp(upPosition, downPosition, k * k);
                    if (k >= 1f) { Current = State.Down; timer = 0f; }
                    break;

                case State.Dead:
                    timer += Time.deltaTime;
                    transform.localPosition = Vector3.Lerp(transform.localPosition, downPosition, Time.deltaTime * 9f);
                    if (timer >= 0.55f) { Current = State.Down; timer = 0f; }
                    break;
            }
        }

        public void Rise()
        {
            if (Current != State.Down) return;
            Current = State.Rising;
            timer = 0f;
            SetCollidersEnabled(true);
        }

        public float DamageMultiplier(Vector3 worldHitPoint)
        {
            Vector3 local = transform.InverseTransformPoint(worldHitPoint);
            float distance = new Vector2(local.x, local.y).magnitude;
            return distance <= criticalRadius ? 2.5f : 1f;
        }

        public void ApplyHit(Vector3 worldHitPoint)
        {
            if (!IsHittable) return;

            bool critical = DamageMultiplier(worldHitPoint) > 1f;
            LastWasCritical = critical;
            LastPoints = critical ? criticalPoints : basePoints;

            Current = State.Dead;
            timer = 0f;
            SetCollidersEnabled(false);
            transform.localRotation = Quaternion.Euler(0f, 0f, UnityEngine.Random.Range(-28f, 28f));

            if (destroyEffect != null)
            {
                var fx = Instantiate(destroyEffect, worldHitPoint, Quaternion.identity);
                Destroy(fx, 2f);
            }

            if (audioSource != null)
            {
                var clip = critical ? criticalClip : hitClip;
                if (clip != null) audioSource.PlayOneShot(clip);
            }

            OnTargetDown?.Invoke(this, LastPoints, critical);
        }

        public void ResetTarget()
        {
            Current = State.Down;
            timer = 0f;
            transform.localPosition = downPosition;
            transform.localRotation = Quaternion.identity;
            SetCollidersEnabled(true);
        }

        private void SetCollidersEnabled(bool enabled)
        {
            if (colliders == null) return;
            foreach (var c in colliders) c.enabled = enabled;
        }

        private static float EaseOutBack(float k)
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1f;
            return 1f + c3 * Mathf.Pow(k - 1f, 3f) + c1 * Mathf.Pow(k - 1f, 2f);
        }
    }
}
