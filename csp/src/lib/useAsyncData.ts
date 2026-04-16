import { type DependencyList, useEffect, useState } from "react";

interface AsyncDataState<T> {
  data: T | null;
  error: string | null;
  loading: boolean;
}

export function useAsyncData<T>(loader: () => Promise<T>, deps: DependencyList = []) {
  const [state, setState] = useState<AsyncDataState<T>>({
    data: null,
    error: null,
    loading: true,
  });
  const [reloadToken, setReloadToken] = useState(0);

  useEffect(() => {
    let cancelled = false;

    async function run() {
      setState((current) => ({
        ...current,
        loading: true,
        error: null,
      }));

      try {
        const data = await loader();

        if (!cancelled) {
          setState({
            data,
            error: null,
            loading: false,
          });
        }
      } catch (error) {
        if (!cancelled) {
          setState({
            data: null,
            error: error instanceof Error ? error.message : "Unexpected error",
            loading: false,
          });
        }
      }
    }

    run();

    return () => {
      cancelled = true;
    };
  }, [reloadToken, ...deps]);

  return {
    ...state,
    reload: () => setReloadToken((current) => current + 1),
  };
}
