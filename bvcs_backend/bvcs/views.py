from pathlib import Path
from django.utils.dateparse import parse_datetime
from django.utils import timezone
from datetime import datetime, timezone as dt_timezone
from rest_framework import status
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework.parsers import MultiPartParser

from .models import Repository, Commit
from .serializers import RepositorySerializer, RepositoryCreateSerializer, CommitSerializer
from .client import BVCSClient, BVCSError


class RepositoryListView(APIView):
    """
    GET  /api/repos/  — list all repositories
    POST /api/repos/  — init a new repository
    """

    def get(self, request):
        repos = Repository.objects.all()
        serializer = RepositorySerializer(repos, many=True)
        return Response(serializer.data)

    def post(self, request):
        serializer = RepositoryCreateSerializer(data=request.data)
        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        name = serializer.validated_data["name"]
        repo_path = BVCSClient.repo_path_for(name)

        if repo_path.exists():
            return Response(
                {"error": f"A repository named '{name}' already exists."},
                status=status.HTTP_400_BAD_REQUEST,
            )

        try:
            repo_path.mkdir(parents=True)
            client = BVCSClient(repo_path)
            client.init()
        except BVCSError as e:
            repo_path.rmdir()
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        repo = Repository.objects.create(
            name=name,
            path=str(repo_path),
        )
        return Response(RepositorySerializer(repo).data, status=status.HTTP_201_CREATED)


class RepositoryDetailView(APIView):
    """
    GET /api/repos/{id}/  — retrieve a single repository with its commits
    """

    def get_repo(self, repo_id):
        try:
            return Repository.objects.get(pk=repo_id)
        except Repository.DoesNotExist:
            return None

    def get(self, request, repo_id):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)
        serializer = RepositorySerializer(repo)
        return Response(serializer.data)


class CommitListView(APIView):
    """
    GET  /api/repos/{id}/commits/  — list commits from bvcs log, sync to DB
    POST /api/repos/{id}/commits/  — create a new commit
    """

    def get_repo(self, repo_id):
        try:
            return Repository.objects.get(pk=repo_id)
        except Repository.DoesNotExist:
            return None

    def get(self, request, repo_id):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)

        try:
            client = BVCSClient(Path(repo.path))
            raw_commits = client.log()
        except BVCSError as e:
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        # Sync commits from bvcs log into the Django DB
        for c in raw_commits:
            ts = datetime.fromtimestamp(c["timestamp"], tz=dt_timezone.utc)
            Commit.objects.get_or_create(
                hash=c["hash"],
                defaults={
                    "repository": repo,
                    "parent_hash": c.get("parent_hash", ""),
                    "blob_hash": c["blob_hash"],
                    "message": c["message"],
                    "timestamp": ts,
                },
            )

        commits = repo.commits.all()
        serializer = CommitSerializer(commits, many=True)
        return Response(serializer.data)

    def post(self, request, repo_id):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)

        message = request.data.get("message", "").strip()
        if not message:
            return Response({"error": "Commit message is required."}, status=status.HTTP_400_BAD_REQUEST)

        try:
            client = BVCSClient(Path(repo.path))
            client.commit(message)
            raw_commits = client.log()
        except BVCSError as e:
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        # The newest commit is first after log()
        latest = raw_commits[0]
        ts = datetime.fromtimestamp(latest["timestamp"], tz=dt_timezone.utc)
        commit, _ = Commit.objects.get_or_create(
            hash=latest["hash"],
            defaults={
                "repository": repo,
                "parent_hash": latest.get("parent_hash", ""),
                "blob_hash": latest["blob_hash"],
                "message": latest["message"],
                "timestamp": ts,
            },
        )
        serializer = CommitSerializer(commit)
        return Response(serializer.data, status=status.HTTP_201_CREATED)


class StageFileView(APIView):
    """
    POST /api/repos/{id}/add/  — stage a file
    Accepts multipart/form-data with a 'file' field.
    """
    parser_classes = [MultiPartParser]

    def get_repo(self, repo_id):
        try:
            return Repository.objects.get(pk=repo_id)
        except Repository.DoesNotExist:
            return None

    def post(self, request, repo_id):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)

        uploaded_file = request.FILES.get("file")
        if not uploaded_file:
            return Response({"error": "No file provided."}, status=status.HTTP_400_BAD_REQUEST)

        max_size = 524288000  # 500MB
        if uploaded_file.size > max_size:
            return Response(
                {"error": f"File too large. Maximum size is 500MB."},
                status=status.HTTP_400_BAD_REQUEST,
            )

        # Write the uploaded file into the repo directory
        repo_path = Path(repo.path)
        dest_path = repo_path / uploaded_file.name
        with open(dest_path, "wb") as f:
            for chunk in uploaded_file.chunks():
                f.write(chunk)

        try:
            client = BVCSClient(repo_path)
            client.add(str(dest_path))
        except BVCSError as e:
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        return Response({"staged": uploaded_file.name}, status=status.HTTP_200_OK)


class CheckoutView(APIView):
    """
    GET /api/repos/{id}/checkout/{hash}/  — retrieve a specific version of a file
    """

    def get_repo(self, repo_id):
        try:
            return Repository.objects.get(pk=repo_id)
        except Repository.DoesNotExist:
            return None

    def get(self, request, repo_id, commit_hash):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)

        repo_path = Path(repo.path)
        output_path = repo_path / f"checkout_{commit_hash[:8]}"

        try:
            client = BVCSClient(repo_path)
            client.checkout(commit_hash, str(output_path))
        except BVCSError as e:
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        return Response({"output_path": str(output_path)}, status=status.HTTP_200_OK)


class StatusView(APIView):
    """
    GET /api/repos/{id}/status/  — current staging status
    """

    def get_repo(self, repo_id):
        try:
            return Repository.objects.get(pk=repo_id)
        except Repository.DoesNotExist:
            return None

    def get(self, request, repo_id):
        repo = self.get_repo(repo_id)
        if repo is None:
            return Response({"error": "Repository not found."}, status=status.HTTP_404_NOT_FOUND)

        try:
            client = BVCSClient(Path(repo.path))
            status_data = client.status()
        except BVCSError as e:
            return Response({"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        return Response(status_data)