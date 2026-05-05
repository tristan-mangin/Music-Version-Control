from rest_framework import serializers
from .models import Repository, Commit


class CommitSerializer(serializers.ModelSerializer):
    class Meta:
        model = Commit
        fields = [
            "hash",
            "parent_hash",
            "blob_hash",
            "message",
            "timestamp",
        ]

class RepositorySerializer(serializers.ModelSerializer):
    commits = CommitSerializer(many=True, read_only=True)

    class Meta:
        model = Repository
        fields = [
            "id",
            "name",
            "path",
            "created_at",
            "commits",
        ]

class RepositoryCreateSerializer(serializers.ModelSerializer):
    """Used only for POST /api/repos/ — accepts name, derives path."""
    class Meta:
        model = Repository
        fields = ["name"]