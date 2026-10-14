const {writeFileSync} = require('node:fs');

module.exports = async ({github, context, core}) => {
  const found = JSON.parse(process.env.FOUND_ARTIFACTS);
  if (found.length !== 1 || found[0].expired) {
    throw new Error('Missing full-build completion artifact');
  }

  const runId = found[0].workflow_run.id;
  const run = (await github.rest.actions.getWorkflowRun({...context.repo, run_id: runId})).data;
  const workflow = (await github.rest.actions.getWorkflow({...context.repo, workflow_id: 'build-binaries.yaml'})).data;
  const fullBuild = run.event === 'workflow_dispatch' || (run.event === 'push' && run.head_branch === 'main');
  if (run.head_sha !== process.env.COMMIT || run.workflow_id !== workflow.id ||
      run.status !== 'completed' || run.conclusion !== 'success' || !fullBuild) {
    throw new Error('Tag requires a successful full main/manual build of this exact commit');
  }

  const artifacts = (await github.paginate(github.rest.actions.listWorkflowRunArtifacts, {...context.repo, run_id: runId}))
    .filter(a => a.name.startsWith('release-'));
  const expected = [
    'release-android-all', 'release-linux-x64', 'release-linux-arm64',
    'release-windows-x64', 'release-windows-arm64', 'release-macos-universal',
    'release-ios-arm64', 'release-ios-simulator-arm64', 'release-web-browser',
  ];
  if (artifacts.some(a => a.expired) || artifacts.map(a => a.name).sort().join() !== expected.sort().join()) {
    throw new Error('Incomplete or expired CI binaries');
  }

  core.setOutput('run_id', runId);
  core.setOutput('artifact_ids', artifacts.map(a => a.id).join(','));
  writeFileSync('BUILD-METADATA.json', JSON.stringify({
    tag: process.env.RELEASE_TAG,
    commit: run.head_sha,
    ci_run: run.html_url,
    artifacts,
  }, null, 2) + '\n');
};
