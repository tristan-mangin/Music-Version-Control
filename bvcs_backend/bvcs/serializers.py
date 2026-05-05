from rest_framework import serializers
from .models import Repository, Commit
import re

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

    def validate_name(self, value):
        if not value.strip():
            raise serializers.ValidationError("Name cannot be blank.")

        if value != value.strip():
            raise serializers.ValidationError("Name cannot have leading or trailing whitespace.")

        if re.search(r'[/\\.]', value):
            raise serializers.ValidationError("Name cannot contain slashes or dots.")

        if not re.match(r'^[a-zA-Z0-9_-]+$', value):
            raise serializers.ValidationError(
                "Name can only contain letters, numbers, hyphens, and underscores."
            )

        if len(value) > 255:
            raise serializers.ValidationError("Name cannot exceed 255 characters.")

        return value