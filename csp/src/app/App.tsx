import { RouterProvider } from "react-router-dom";
import { router } from "./router";
import { useIntentionalClickGuard } from "../lib/useIntentionalClickGuard";

export function App() {
  useIntentionalClickGuard();

  return <RouterProvider router={router} />;
}
