import { listBorderRouters } from "../api/nodes";
import { BorderRouterTable } from "../components/common/BorderRouterTable";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { useAsyncData } from "../lib/useAsyncData";

export function BorderRoutersPage() {
  const { data, error, loading } = useAsyncData(listBorderRouters, []);

  if (loading) {
    return <LoadingState label="Loading border-router inventory..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load border routers" message={error ?? "Border-router inventory is unavailable."} />;
  }

  return (
    <PageContainer>
      <div className="card">
        {data.length > 0 ? (
          <BorderRouterTable borderRouters={data} />
        ) : (
          <EmptyState title="No border routers" message="No border-router registrations have been recorded yet." />
        )}
      </div>
    </PageContainer>
  );
}
