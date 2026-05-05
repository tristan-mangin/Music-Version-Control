import subprocess 
import json
from pathlib import Path
from django.conf import settings

class BVCSError(Exception):
    '''Raised when the bvcs command returns a non-zero exit code'''
    pass

class BVCSClient:
    def __init__(self, repo_path: Path):
        self.repo_path = repo_path
        self.binary = settings.BVCS_BINARY_PATH

    def _run(self, args: list[str]) -> str:
        '''
        Run a bvcs command in the context of this repo. 
        Returns stdout as a string on success.
        Raises BVCSError on non-zero exit code, with stderr as the error message.
        '''
        result = subprocess.run(
            [self.binary] + args, 
            capture_output=True,
            text=True,
            cwd=self.repo_path,
        )

        if result.returncode != 0: 
            raise BVCSError(result.stderr.strip())

        return result.stdout.strip()

    def init(self) -> None:
        '''Initialize a new repository'''
        self._run(['init'])

    def add(self, file_path: str) -> None:
        '''Add a file to the staging area'''
        self._run(['add', file_path])

    def commit(self, message: str) -> str:
        '''Returns stdout from the commit command (e.g. the new commit hash)'''
        return self._run(['commit', '-m', message])

    def log(self) -> list[dict]:
        '''
        Returns commit history as a list of dicts.
        Requires --format=json output from bvcs binary
        '''
        output = self._run(["log", "--format=json"])
        data = json.loads(output)
        return data["commits"]

    def status(self) -> dict:
        '''
        Returns current staging status as a dict.
        Requires --format=json output from bvcs binary
        '''
        output = self._run(["status", "--format=json"])
        data = json.loads(output)
        return data["status"]

    def checkout(self, commit_hash: str, output_path: str) -> None:
        '''Checkout a specific commit to the given output path'''
        self._run(['checkout', commit_hash, output_path])

    @staticmethod
    def repo_path_for(repo_name: str) -> Path:
        '''Derives the absolute repo path from a repo name'''
        return Path(settings.BVCS_REPOS_ROOT) / repo_name