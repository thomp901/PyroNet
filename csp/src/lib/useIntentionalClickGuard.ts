import { useEffect } from "react";

const maxClickDurationMs = 250;
const maxPointerTravelPx = 8;
const suppressClickWindowMs = 400;

function hasExpandedSelection() {
  const selection = window.getSelection();
  return selection !== null && !selection.isCollapsed && selection.toString().trim().length > 0;
}

function isNavigationalTarget(target: EventTarget | null) {
  return target instanceof Element && target.closest("a[href], [role='link']") !== null;
}

export function useIntentionalClickGuard() {
  useEffect(() => {
    const interactionState = {
      activePointerId: -1,
      pointerDown: false,
      startX: 0,
      startY: 0,
      startTime: 0,
      maxDistance: 0,
      suppressUntil: 0,
    };

    function resetPointerTracking() {
      interactionState.activePointerId = -1;
      interactionState.pointerDown = false;
      interactionState.startX = 0;
      interactionState.startY = 0;
      interactionState.startTime = 0;
      interactionState.maxDistance = 0;
    }

    function handlePointerDown(event: PointerEvent) {
      if (!event.isPrimary || event.button !== 0) {
        return;
      }

      interactionState.activePointerId = event.pointerId;
      interactionState.pointerDown = true;
      interactionState.startX = event.clientX;
      interactionState.startY = event.clientY;
      interactionState.startTime = performance.now();
      interactionState.maxDistance = 0;
      interactionState.suppressUntil = 0;
    }

    function handlePointerMove(event: PointerEvent) {
      if (!interactionState.pointerDown || interactionState.activePointerId !== event.pointerId) {
        return;
      }

      const deltaX = event.clientX - interactionState.startX;
      const deltaY = event.clientY - interactionState.startY;
      interactionState.maxDistance = Math.max(interactionState.maxDistance, Math.hypot(deltaX, deltaY));
    }

    function handlePointerUp(event: PointerEvent) {
      if (!interactionState.pointerDown || interactionState.activePointerId !== event.pointerId) {
        return;
      }

      const interactionDuration = performance.now() - interactionState.startTime;
      const shouldSuppressClick =
        hasExpandedSelection() || interactionState.maxDistance > maxPointerTravelPx || interactionDuration > maxClickDurationMs;

      if (shouldSuppressClick) {
        interactionState.suppressUntil = performance.now() + suppressClickWindowMs;
      }

      resetPointerTracking();
    }

    function handlePointerCancel(event: PointerEvent) {
      if (interactionState.activePointerId === event.pointerId) {
        resetPointerTracking();
      }
    }

    function handleClick(event: MouseEvent) {
      if (event.detail === 0) {
        return;
      }

      if (!isNavigationalTarget(event.target)) {
        return;
      }

      if (performance.now() > interactionState.suppressUntil) {
        interactionState.suppressUntil = 0;
        return;
      }

      interactionState.suppressUntil = 0;
      event.preventDefault();
      event.stopPropagation();
    }

    document.addEventListener("pointerdown", handlePointerDown, true);
    document.addEventListener("pointermove", handlePointerMove, true);
    document.addEventListener("pointerup", handlePointerUp, true);
    document.addEventListener("pointercancel", handlePointerCancel, true);
    document.addEventListener("click", handleClick, true);

    return () => {
      document.removeEventListener("pointerdown", handlePointerDown, true);
      document.removeEventListener("pointermove", handlePointerMove, true);
      document.removeEventListener("pointerup", handlePointerUp, true);
      document.removeEventListener("pointercancel", handlePointerCancel, true);
      document.removeEventListener("click", handleClick, true);
    };
  }, []);
}
