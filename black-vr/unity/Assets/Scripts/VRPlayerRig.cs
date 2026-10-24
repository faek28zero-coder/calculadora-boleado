// ============================================================
//  BLACK VR — VRPlayerRig
//  Rig de jogador para OpenXR: locomoção, snap turn e fallback
//  para desktop (WASD + mouse), para testar sem headset.
//  Requer: XR Interaction Toolkit 2.5+ (com. unity.xr.interaction.toolkit)
// ============================================================
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.XR.Interaction.Toolkit;
using UnityEngine.XR.Interaction.Toolkit.Locomotion;
using UnityEngine.XR.Interaction.Toolkit.Locomotion.Movement;
using UnityEngine.XR.Interaction.Toolkit.Locomotion.Turning;

namespace BlackVR
{
    [DisallowMultipleComponent]
    public class VRPlayerRig : MonoBehaviour
    {
        [Header("Referências do rig")]
        [Tooltip("Objeto que desce até o chão real (altura do usuário).")]
        [SerializeField] private Transform rigRoot;
        [Tooltip("Câmera usada como olhos do jogador.")]
        [SerializeField] private Transform headCamera;
        [Tooltip("Origem de locomoção (XR Origin).")]
        [SerializeField] private XROrigin xrOrigin;

        [Header("Locomoção")]
        [SerializeField] private float moveSpeed = 3.4f;
        [SerializeField] private float snapTurnDegrees = 30f;
        [SerializeField] private float snapTurnCooldown = 0.3f;
        [SerializeField] private float gravity = -18f;

        [Header("Desktop (debug sem headset)")]
        [SerializeField] private bool enableDesktopFallback = true;
        [SerializeField] private float mouseSensitivity = 2.2f;
        [SerializeField] private float desktopHeight = 1.68f;

        [Header("Limites da arena")]
        [SerializeField] private Vector2 boundsX = new Vector2(-13f, 13f);
        [SerializeField] private Vector2 boundsZ = new Vector2(-9.5f, 21.5f);

        private CharacterController controller;
        private ActionBasedContinuousMoveProvider moveProvider;
        private ActionBasedSnapTurnProvider snapTurnProvider;
        private float verticalVelocity;
        private float lastTurnTime;
        private float yaw;
        private float pitch;

        public bool IsDesktop => !IsXRPresent();
        public Transform Head => headCamera;
        public XROrigin Origin => xrOrigin;

        private void Awake()
        {
            if (rigRoot == null) rigRoot = transform;
            if (headCamera == null && xrOrigin != null) headCamera = xrOrigin.Camera.transform;
            if (xrOrigin == null) xrOrigin = GetComponentInChildren<XROrigin>();

            controller = rigRoot.GetComponent<CharacterController>();
            if (controller == null)
                controller = rigRoot.gameObject.AddComponent<CharacterController>();

            controller.height = 1.8f;
            controller.radius = 0.32f;
            controller.center = new Vector3(0f, 0.9f, 0f);
            controller.stepOffset = 0.35f;
            controller.slopeLimit = 50f;

            moveProvider = GetComponentInChildren<ActionBasedContinuousMoveProvider>(true);
            snapTurnProvider = GetComponentInChildren<ActionBasedSnapTurnProvider>(true);

            if (moveProvider != null) moveProvider.moveSpeed = moveSpeed;
            if (snapTurnProvider != null)
            {
                snapTurnProvider.turnAmount = snapTurnDegrees;
                snapTurnProvider.debounceTime = snapTurnCooldown;
            }
        }

        private void Start()
        {
            SnapToFloor();
        }

        /// <summary>Coloca o rig na altura correta (piso real / altura padrão).</summary>
        public void SnapToFloor()
        {
            if (rigRoot == null) return;
            var p = rigRoot.position;
            if (IsDesktop && enableDesktopFallback)
                p.y = desktopHeight;
            rigRoot.position = p;
        }

        /// <summary>Teleporta o jogador para um ponto seguro (início de round).</summary>
        public void TeleportTo(Vector3 worldPosition, float yawDegrees = 0f)
        {
            if (controller != null) controller.enabled = false;
            rigRoot.position = new Vector3(worldPosition.x, worldPosition.y, worldPosition.z);
            rigRoot.rotation = Quaternion.Euler(0f, yawDegrees, 0f);
            if (controller != null) controller.enabled = true;
            verticalVelocity = 0f;
            yaw = yawDegrees;
            pitch = 0f;
        }

        private void Update()
        {
            if (IsDesktop && enableDesktopFallback)
                UpdateDesktop();

            ClampToBounds();
            ApplyGravity();
        }

        private void ApplyGravity()
        {
            if (controller == null || !controller.enabled) return;

            if (controller.isGrounded && verticalVelocity < 0f)
                verticalVelocity = -2f;
            else
                verticalVelocity += gravity * Time.deltaTime;

            var motion = new Vector3(0f, verticalVelocity, 0f);
            controller.Move(motion * Time.deltaTime);
        }

        private void ClampToBounds()
        {
            var p = rigRoot.position;
            p.x = Mathf.Clamp(p.x, boundsX.x, boundsX.y);
            p.z = Mathf.Clamp(p.z, boundsZ.x, boundsZ.y);
            rigRoot.position = p;
        }

        // ------------------------------------------------------------ desktop
        private void UpdateDesktop()
        {
            var kb = Keyboard.current;
            var mouse = Mouse.current;
            if (kb == null) return;

            // olhar com o mouse (segurando o botão direito evita capturar o cursor em edit-mode)
            if (mouse != null && mouse.rightButton.isPressed)
            {
                yaw += mouse.delta.ReadValue().x * mouseSensitivity * 0.1f;
                pitch -= mouse.delta.ReadValue().y * mouseSensitivity * 0.1f;
                pitch = Mathf.Clamp(pitch, -85f, 85f);
                rigRoot.rotation = Quaternion.Euler(0f, yaw, 0f);
                if (headCamera != null)
                    headCamera.localRotation = Quaternion.Euler(pitch, 0f, 0f);
            }

            var input = Vector2.zero;
            if (kb.wKey.isPressed || kb.upArrowKey.isPressed) input.y += 1f;
            if (kb.sKey.isPressed || kb.downArrowKey.isPressed) input.y -= 1f;
            if (kb.dKey.isPressed || kb.rightArrowKey.isPressed) input.x += 1f;
            if (kb.aKey.isPressed || kb.leftArrowKey.isPressed) input.x -= 1f;

            if (input.sqrMagnitude > 0.001f && controller != null && controller.enabled)
            {
                var forward = rigRoot.forward;
                forward.y = 0f;
                forward.Normalize();
                var right = rigRoot.right;
                right.y = 0f;
                right.Normalize();
                var move = (forward * input.y + right * input.x).normalized * moveSpeed;
                controller.Move(move * Time.deltaTime);
            }
        }

        private static bool IsXRPresent()
        {
            var devices = UnityEngine.XR.InputDevices.GetDevicesWithCharacteristics(
                UnityEngine.XR.InputDeviceCharacteristics.HeadMounted);
            return devices.Count > 0;
        }
    }
}
