from django.contrib import admin
from .models import Repository, Commit

@admin.register(Repository)
class RepositoryAdmin(admin.ModelAdmin):
    list_display = ["id", "name", "path", "created_at"]
    search_fields = ["name"]
    ordering = ["name"]

@admin.register(Commit)
class CommitAdmin(admin.ModelAdmin):
    list_display = ["hash_short", "repository", "message", "timestamp"]
    search_fields = ["hash", "message"]
    list_filter = ["repository"]
    ordering = ["-timestamp"]

    def hash_short(self, obj):
        return obj.hash[:8]
    hash_short.short_description = "Hash"