from unittest.mock import patch, MagicMock
from pathlib import Path
import json
import subprocess
import unittest.mock

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

# View tests

class TestRepositoryListView(APITestCase):
    @patch('bvcs.views.BVCSClient')
    def test_create_repository_success(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.init.return_value = None

        fake_path = MagicMock()
        fake_path.exists.return_value = False
        fake_path.__str__ = lambda self: '/fake/path/new-repo'
        MockClient.repo_path_for.return_value = fake_path

        response = self.client.post('/api/repos/', {'name': 'new-repo'}, format='json')

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['name'], 'new-repo')
        self.assertTrue(Repository.objects.filter(name='new-repo').exists())

    @patch('bvcs.views.BVCSClient')
    def test_create_repository_already_exists(self, MockClient):
        with patch('bvcs.views.Path.exists', return_value=True):
            response = self.client.post('/api/repos/', {'name': 'existing-repo'}, format='json')

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn('error', response.data)

    def test_create_repository_missing_name(self):
        response = self.client.post('/api/repos/', {}, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_list_repositories_empty(self):
        response = self.client.get('/api/repos/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data, [])

    def test_list_repositories_returns_all(self):
        Repository.objects.create(name='repo-a', path='/some/path/a')
        Repository.objects.create(name='repo-b', path='/some/path/b')
        response = self.client.get('/api/repos/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(len(response.data), 2)

    @patch('bvcs.views.BVCSClient')
    def test_create_repository_init_failure_cleans_up(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.init.side_effect = BVCSError('init failed')

        fake_path = MagicMock()
        fake_path.exists.return_value = False
        fake_path.__str__ = lambda self: '/fake/path/bad-repo'
        MockClient.repo_path_for.return_value = fake_path

        response = self.client.post('/api/repos/', {'name': 'bad-repo'}, format='json')

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)
        fake_path.rmdir.assert_called_once()
        self.assertFalse(Repository.objects.filter(name='bad-repo').exists())


class TestRepositoryDetailView(APITestCase):
    def setUp(self):
        self.repo = Repository.objects.create(name='my-repo', path='/some/path')

    def test_get_existing_repository(self):
        response = self.client.get(f'/api/repos/{self.repo.id}/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['name'], 'my-repo')

    def test_get_nonexistent_repository(self):
        response = self.client.get('/api/repos/99999/')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)


class TestCommitListView(APITestCase):
    def setUp(self):
        self.repo = Repository.objects.create(name='my-repo', path='/some/path')
        self.sample_commits = [
            {
                "hash": "abc123",
                "parent_hash": "",
                "blob_hash": "def456",
                "timestamp": 1000000,
                "message": "first commit"
            }
        ]

    @patch('bvcs.views.BVCSClient')
    def test_get_commits_syncs_to_db(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.log.return_value = self.sample_commits

        response = self.client.get(f'/api/repos/{self.repo.id}/commits/')

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(len(response.data), 1)
        self.assertTrue(Commit.objects.filter(hash='abc123').exists())

    @patch('bvcs.views.BVCSClient')
    def test_get_commits_idempotent(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.log.return_value = self.sample_commits

        self.client.get(f'/api/repos/{self.repo.id}/commits/')
        self.client.get(f'/api/repos/{self.repo.id}/commits/')

        self.assertEqual(Commit.objects.filter(hash='abc123').count(), 1)

    @patch('bvcs.views.BVCSClient')
    def test_create_commit_success(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.commit.return_value = 'abc123'
        mock_instance.log.return_value = self.sample_commits

        response = self.client.post(
            f'/api/repos/{self.repo.id}/commits/',
            {'message': 'first commit'},
            format='json'
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['hash'], 'abc123')

    @patch('bvcs.views.BVCSClient')
    def test_create_commit_missing_message(self, MockClient):
        response = self.client.post(
            f'/api/repos/{self.repo.id}/commits/',
            {},
            format='json'
        )
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    @patch('bvcs.views.BVCSClient')
    def test_get_commits_bvcs_error(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.log.side_effect = BVCSError('log failed')

        response = self.client.get(f'/api/repos/{self.repo.id}/commits/')
        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_get_commits_nonexistent_repo(self):
        response = self.client.get('/api/repos/99999/commits/')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)


class TestStageFileView(APITestCase):
    def setUp(self):
        self.repo = Repository.objects.create(name='my-repo', path='/some/path')

    @patch('bvcs.views.BVCSClient')
    def test_stage_file_success(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.add.return_value = None

        from django.core.files.uploadedfile import SimpleUploadedFile
        test_file = SimpleUploadedFile('test.wav', b'audio data', content_type='audio/wav')

        with patch('builtins.open', unittest.mock.mock_open()), \
             patch('bvcs.views.Path.mkdir'):
            response = self.client.post(
                f'/api/repos/{self.repo.id}/add/',
                {'file': test_file},
                format='multipart'
            )

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['staged'], 'test.wav')

    def test_stage_file_no_file_provided(self):
        response = self.client.post(f'/api/repos/{self.repo.id}/add/', {}, format='multipart')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_stage_file_nonexistent_repo(self):
        response = self.client.post('/api/repos/99999/add/', {}, format='multipart')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)


class TestCheckoutView(APITestCase):
    def setUp(self):
        self.repo = Repository.objects.create(name='my-repo', path='/some/path')

    @patch('bvcs.views.BVCSClient')
    def test_checkout_success(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.checkout.return_value = None

        response = self.client.get(f'/api/repos/{self.repo.id}/checkout/abc123/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertIn('output_path', response.data)

    @patch('bvcs.views.BVCSClient')
    def test_checkout_bvcs_error(self, MockClient):
        mock_instance = MockClient.return_value
        mock_instance.checkout.side_effect = BVCSError('checkout failed')

        response = self.client.get(f'/api/repos/{self.repo.id}/checkout/abc123/')
        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_checkout_nonexistent_repo(self):
        response = self.client.get('/api/repos/99999/checkout/abc123/')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)