// ============================================================
//  BLACK VR — XRWeapon
//  Arma hitscan para VR: mira pelo controlador, munição,
//  recarga, recuo, retroalimentação háptica e traçante.
// ============================================================
using System;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit.Inputs.Haptics;

namespace BlackVR
{
    [RequireComponent(typeof(AudioSource))]
    public class XRWeapon : MonoBehaviour
    {
        [Header("Referências")]
        [Tooltip("Transform do controlador que segura a arma.")]
        [SerializeField] private Transform muzzle;
        [Tooltip("Controlador XR associado (para vibração).")]
        [SerializeField] private HapticImpulsePlayer haptics;
        [SerializeField] private LineRenderer tracerPrefab;
        [SerializeField] private ParticleSystem muzzleFlash;
        [SerializeField] private GameObject impactPrefab;

        [Header("Configuração de tiro")]
        [SerializeField] private int magazineSize = 12;
        [SerializeField] private float fireInterval = 0.11f;
        [SerializeField] private float range = 120f;
        [SerializeField] private LayerMask hitMask = ~0;
        [SerializeField] private float baseDamage = 1f;

        [Header("Recarga")]
        [SerializeField] private float reloadTime = 1.35f;

        [Header("Sensações")]
        [SerializeField] private float recoilKick = 0.035f;
        [SerializeField] private float recoilRotation = 4.5f;
        [SerializeField] private float hapticAmplitude = 0.55f;
        [SerializeField] private float hapticDuration = 0.06f;
        [SerializeField] private AudioClip shotClip;
        [SerializeField] private AudioClip dryClip;
        [SerializeField] private AudioClip reloadClip;
        [SerializeField] private AudioClip impactClip;

        public int Ammo { get; private set; }
        public int MagazineSize => magazineSize;
        public bool IsReloading { get; private set; }
        public event Action<RaycastHit, float> OnHit;   // (hit, dano)
        public event Action OnShot;
        public event Action<int> OnAmmoChanged;

        private AudioSource audioSource;
        private float nextShotTime;
        private float reloadEndTime;
        private Vector3 restLocalPosition;
        private Quaternion restLocalRotation;
        private bool roundActive = true;

        private void Awake()
        {
            audioSource = GetComponent<AudioSource>();
            if (muzzle == null) muzzle = transform;
            restLocalPosition = transform.localPosition;
            restLocalRotation = transform.localRotation;
            Ammo = magazineSize;
        }

        private void OnEnable()
        {
            if (haptics == null) haptics = GetComponentInParent<HapticImpulsePlayer>();
        }

        public void SetRoundActive(bool active)
        {
            roundActive = active;
            if (!active) IsReloading = false;
        }

        public void TriggerPressed()
        {
            if (!roundActive) return;
            if (IsReloading) return;
            if (Time.time < nextShotTime) return;

            if (Ammo <= 0)
            {
                Play(dryClip);
                BeginReload();
                return;
            }

            Fire();
        }

        public void TriggerReleased()
        {
            // armas automáticas: use TriggerPressed em loop; aqui só liberamos o feedback
        }

        private void Fire()
        {
            nextShotTime = Time.time + fireInterval;
            Ammo--;
            OnAmmoChanged?.Invoke(Ammo);
            OnShot?.Invoke();

            Play(shotClip);
            if (muzzleFlash != null) muzzleFlash.Play();
            Buzz();

            Vector3 origin = muzzle.position;
            Vector3 direction = muzzle.forward;

            if (Physics.Raycast(origin, direction, out RaycastHit hit, range, hitMask, QueryTriggerInteraction.Ignore))
            {
                SpawnTracer(origin, hit.point);
                if (impactPrefab != null)
                {
                    var fx = Instantiate(impactPrefab, hit.point, Quaternion.LookRotation(hit.normal));
                    Destroy(fx, 2f);
                }

                var target = hit.collider.GetComponentInParent<Target>();
                if (target != null && target.IsHittable)
                {
                    float damage = baseDamage * target.DamageMultiplier(hit.point);
                    target.ApplyHit(hit.point);
                    OnHit?.Invoke(hit, damage);
                }
                else
                {
                    OnHit?.Invoke(hit, 0f);
                    if (impactClip != null) Play(impactClip);
                }
            }
            else
            {
                SpawnTracer(origin, origin + direction * range);
            }

            ApplyRecoil();

            if (Ammo <= 0) BeginReload();
        }

        public void BeginReload()
        {
            if (IsReloading || Ammo == magazineSize || !roundActive) return;
            IsReloading = true;
            reloadEndTime = Time.time + reloadTime;
            Play(reloadClip);
            OnAmmoChanged?.Invoke(Ammo);
        }

        private void Update()
        {
            if (IsReloading && Time.time >= reloadEndTime)
            {
                IsReloading = false;
                Ammo = magazineSize;
                OnAmmoChanged?.Invoke(Ammo);
            }

            // volta suave para a pose de descanso (recuo)
            transform.localPosition = Vector3.Lerp(transform.localPosition, restLocalPosition, Time.deltaTime * 12f);
            transform.localRotation = Quaternion.Slerp(transform.localRotation, restLocalRotation, Time.deltaTime * 10f);

            if (IsReloading)
            {
                float k = Mathf.Sin(Mathf.Clamp01(1f - (reloadEndTime - Time.time) / reloadTime) * Mathf.PI);
                transform.localRotation = restLocalRotation * Quaternion.Euler(-35f * k, 12f * k, 8f * k);
            }
        }

        private void ApplyRecoil()
        {
            transform.localPosition = restLocalPosition + new Vector3(
                UnityEngine.Random.Range(-0.004f, 0.004f),
                UnityEngine.Random.Range(0f, 0.006f),
                -recoilKick);

            float kick = recoilRotation * UnityEngine.Random.Range(0.7f, 1.2f);
            transform.localRotation = restLocalRotation * Quaternion.Euler(-kick, UnityEngine.Random.Range(-1.2f, 1.2f), 0f);
        }

        private void SpawnTracer(Vector3 from, Vector3 to)
        {
            if (tracerPrefab == null) return;
            var tracer = Instantiate(tracerPrefab);
            tracer.positionCount = 2;
            tracer.SetPosition(0, from);
            tracer.SetPosition(1, to);
            Destroy(tracer.gameObject, 0.06f);
        }

        private void Buzz()
        {
            if (haptics != null) haptics.SendHapticImpulse(hapticAmplitude, hapticDuration);
        }

        private void Play(AudioClip clip)
        {
            if (clip != null && audioSource != null) audioSource.PlayOneShot(clip);
        }
    }
}
