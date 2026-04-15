import { listNodes } from "../api/nodes";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { NodeFleetTable } from "../components/common/NodeFleetTable";
import { PageContainer } from "../components/common/PageContainer";
import { useAsyncData } from "../lib/useAsyncData";

export function NodesPage() {
  const { data, error, loading } = useAsyncData(listNodes, []);

  if (loading) {
    return <LoadingState label="Loading node inventory..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load nodes" message={error ?? "Node inventory is unavailable."} />;
  }

  return (
    <PageContainer>
      <div className="card">
        <NodeFleetTable nodes={data} sortable showNodeIdLink={false} />
      </div>
    </PageContainer>
  );
}
