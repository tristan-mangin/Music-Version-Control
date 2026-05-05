from unittest.mock import patch, MagicMock
from pathlib import Path
import json
import subprocess

from django.test import TestCase
from rest_framework.test import APITestCase
from rest_framework import status

from .client import BVCSClient, BVCSError
from .models import Repository, Commit

BINARY = '/mnt/c/Code/Music-Version-Control/build/bvcs'
REPO_PATH = Path('/mnt/c/Code/Music-Version-Control/repos/test_repo')

# BVCSClient tests
class TestBVCSClient(TestCase):
    def setUp(self):
        self.client = BVCSClient(REPO_PATH)

    @patch('bvcs.client.subprocess.run')
    def test_run_returns_stdout_on_success(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='output\n', stderr='')
        result = self.client._run(['log'])
        self.assertEqual(result, 'output')

    @patch('bvcs.client.subprocess.run')
    def test_run_raises_bvcs_error_on_failure(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1, stdout='', stderr='something went wrong')
        with self.assertRaises(BVCSError) as ctx:
            self.client._run(['log'])
        self.assertEqual(str(ctx.exception), 'something went wrong')

    @patch('bvcs.client.subprocess.run')
    def test_run_passes_correct_args(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='', stderr='')
        self.client._run(['commit', '-m', 'test message'])
        mock_run.assert_called_once_with(
            [BINARY, 'commit', '-m', 'test message'],
            capture_output=True,
            text=True,
            cwd=REPO_PATH,
        )

class TestBVCSClientCommands(TestCase):
    def setUp(self):
        self.client = BVCSClient(REPO_PATH)

    @patch('bvcs.client.subprocess.run')
    def test_init(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='', stderr='')
        self.client.init()
        args = mock_run.call_args[0][0]
        self.assertEqual(args, [BINARY, 'init'])

    @patch('bvcs.client.subprocess.run')
    def test_add(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='', stderr='')
        self.client.add('/some/file.wav')
        args = mock_run.call_args[0][0]
        self.assertEqual(args, [BINARY, 'add', '/some/file.wav'])

    @patch('bvcs.client.subprocess.run')
    def test_commit(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='abc123', stderr='')
        result = self.client.commit('my message')
        args = mock_run.call_args[0][0]
        self.assertEqual(args, [BINARY, 'commit', '-m', 'my message'])
        self.assertEqual(result, 'abc123')

    @patch('bvcs.client.subprocess.run')
    def test_log_parses_json(self, mock_run):
        payload = {"commits": [{"hash": "abc", "parent_hash": "", "blob_hash": "def", "timestamp": 1000, "message": "m"}]}
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(payload), stderr='')
        result = self.client.log()
        self.assertEqual(result, payload["commits"])

    @patch('bvcs.client.subprocess.run')
    def test_checkout(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='', stderr='')
        self.client.checkout('abc123', '/output/path')
        args = mock_run.call_args[0][0]
        self.assertEqual(args, [BINARY, 'checkout', 'abc123', '/output/path'])

    @patch('bvcs.client.subprocess.run')
    def test_log_raises_on_invalid_json(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0, stdout='not json', stderr='')
        with self.assertRaises(json.JSONDecodeError):
            self.client.log()


class TestBVCSClientStaticMethods(TestCase):
    def test_repo_path_for(self):
        path = BVCSClient.repo_path_for('my-repo')
        self.assertIn('my-repo', str(path))